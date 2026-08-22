#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define PORT "8888"
#define BACKLOG 5 // maximum number of pending connections in the queue
#define DATA_BUF_SIZE 1000

typedef struct {
	char name[50];
	int score;
	bool board_wrapping;
	int board_width;
	int board_height;
} HighScoreEntry;

size_t parse_post_prefix(char const *start_req, size_t num_bytes);
size_t parse_uri(char const *start_of_uri, size_t num_bytes);
size_t parse_request_line(char const *start_req, size_t num_bytes);
size_t parse_http_version(char const *start_req, size_t num_bytes);
size_t parse_content_length(char const *data, size_t num_bytes);

char const *locate_string_bounded(char const *haystack, size_t nbytes_hay,
				  char const *needle, size_t nbytes_needle);

int parse_json(char const *body, HighScoreEntry *e, size_t max_name_str_size);

void append_to_file(char *filename, char *data, int size);

int main(void) {

	// sockaddr_in is identical to sockaddr.
	// sockaddr_in (in stands for internet)
	// was created for convinience
	// in order to reference the elements of the socket address.

	// Given this is the server application we want to initialize a
	// stream socket, bind to 8888 port and listen to incomming
	// connections from any ip.

	struct addrinfo hints, *result, *p;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // dont care if we use ipv4 or ipv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // I intend to listen to everyone
				     // (all adresses and
	// interfaces), give me the wildcard adress (0.0.0.0)

	if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
		fprintf(stderr, "unable to fill out information for "
				"server socket\n");
		exit(EXIT_FAILURE);
	}

	int s;
	// go through each element in the linked list and show IP's
	for (p = result; p != NULL; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s == -1) {
			// fprintf(stderr, "Could not create a socket\n");
			continue;
		}

		int yes = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes,
			       sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}

		// we need to bind the socket to the port we are going
		// to use so that the kernel knows where (to what
		// process it should route the network packets coming
		// in).
		if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
			// fprintf(stderr, "Could not bind the socket\n");
			continue;
		}
		break;
	}
	freeaddrinfo(result);
	if (p == NULL) {
		fprintf(stderr, "Server failed to bind\n");
		exit(EXIT_FAILURE);
	}

	printf("listening...\n");
	if (listen(s, BACKLOG) == -1) {
		fprintf(stderr, "Could not listen\n");
		exit(EXIT_FAILURE);
	}

	socklen_t addr_size = sizeof(struct sockaddr_storage);
	struct sockaddr_storage client_addr;
	int receive_s;
	printf("waiting for client...\n");
	receive_s = accept(s, (struct sockaddr *)&client_addr, &addr_size);
	if (receive_s == -1) {
		fprintf(stderr, "Could not accept\n");
		exit(EXIT_FAILURE);
	}
	char data[DATA_BUF_SIZE + 1] = "";
	int num_bytes;
	num_bytes = recv(receive_s, data, DATA_BUF_SIZE, 0);
	printf("recv() returned %d bytes\n", num_bytes);

	char const *cursor = data;
	size_t parsed;
	size_t bytes_left = num_bytes;
	size_t content_size;
	if (num_bytes > 0) {
		data[num_bytes] = '\0';
		printf("%s\n", data);
		parsed = parse_request_line(cursor, bytes_left);
		if (parsed == 0)
			goto close_conn_err;
		cursor += parsed;
		bytes_left -= parsed;
		content_size = parse_content_length(cursor, bytes_left);
		if (content_size > 0) {
			printf("number of content bytes: %lu\n", content_size);
			char const end_of_headers[] = "\r\n\r\n";
			cursor = locate_string_bounded(
			    cursor, bytes_left, end_of_headers,
			    sizeof end_of_headers - 1);
			if (cursor == NULL)
				goto close_conn_err;
			cursor += sizeof end_of_headers - 1;
			// start of body
			bytes_left = content_size;

			HighScoreEntry e;
			if (parse_json(cursor, &e, sizeof e.name) == 0) {
				char const all_ok[] = "HTTP/1.1 200 OK\r\n\r\n";
				if (send(receive_s, all_ok, sizeof all_ok - 1,
					 0) == -1)
					fprintf(stderr,
						"There was an issue sending "
						"the response\n");
				else
					printf("sent all ok response\n");
				close(receive_s);
			}
			printf("parsed name: %s\nparsed score: %u\n", e.name,
			       e.score);
			char score_entry[250] = "";
			// TODO: Store unix time as well
			sprintf(score_entry, "%s,%u,%d,%d,%d\n", e.name,
				e.score, e.board_wrapping, e.board_width,
				e.board_height);
			printf("body received: %s\n", cursor);
			append_to_file("highscores.csv", score_entry,
				       strlen(score_entry));
		}
	close_conn_err:
		// send(receive_s, , size_t n, int flags);
		close(receive_s);
	}
	close(s);
	return EXIT_SUCCESS;
}

int parse_json(char const *body, HighScoreEntry *e, size_t max_name_str_size) {

	int status = 0;
	cJSON *json_data = cJSON_Parse(body);
	if (json_data == NULL) {
		const char *error_ptr = cJSON_GetErrorPtr();
		if (error_ptr != NULL) {
			fprintf(stderr, "Error before: %s\n", error_ptr);
		}
		status = 0;
		goto end;
	}
	const cJSON *name = NULL;
	const cJSON *score = NULL;
	const cJSON *wrapping = NULL;
	const cJSON *width = NULL;
	const cJSON *height = NULL;

	name = cJSON_GetObjectItemCaseSensitive(json_data, "name");
	if (cJSON_IsString(name) && (name->valuestring != NULL)) {
		if (strlen(name->valuestring) < max_name_str_size)
			memcpy(e->name, name->valuestring,
			       strlen(name->valuestring));
	}

	score = cJSON_GetObjectItemCaseSensitive(json_data, "score");
	if (cJSON_IsNumber(score) && (score->valueint >= 0)) {
		e->score = score->valueint;
	}

	wrapping =
	    cJSON_GetObjectItemCaseSensitive(json_data, "board wrapping");
	if (cJSON_IsNumber(wrapping) &&
	    ((wrapping->valueint == 1) || (wrapping->valueint == 0))) {
		e->score = score->valueint;
	}

	width = cJSON_GetObjectItemCaseSensitive(json_data, "board width");
	if (cJSON_IsNumber(width) && (width->valueint >= 2)) {
		e->board_width = width->valueint;
	}

	height = cJSON_GetObjectItemCaseSensitive(json_data, "board height");
	if (cJSON_IsNumber(height) && (height->valueint >= 2)) {
		e->board_height = height->valueint;
	}

end:
	cJSON_Delete(json_data);
	return status;
}

size_t parse_post_prefix(char const *start_req, size_t num_bytes) {
	static char const post_method[] = "POST ";
	if (num_bytes < sizeof(post_method) - 1)
		return 0;
	if (memcmp(start_req, post_method, sizeof(post_method) - 1) == 0)
		return sizeof(post_method) - 1;
	return 0;
}

size_t parse_uri(char const *start_of_uri, size_t num_bytes) {
	static char const uri[] = "/submit_score ";
	if (num_bytes < sizeof(uri) - 1)
		return 0;
	if (memcmp(start_of_uri, uri, sizeof(uri) - 1) == 0)
		return sizeof(uri) - 1;
	return 0;
}

size_t parse_http_version(char const *start_http_ver, size_t num_bytes) {
	static char const v11[] = "HTTP/1.1\r\n";
	if (num_bytes < sizeof(v11) - 1)
		return 0;
	if (memcmp(start_http_ver, v11, sizeof(v11) - 1) == 0)
		return sizeof(v11) - 1;
	return 0;
}

size_t parse_request_line(char const *start_req, size_t num_bytes) {
	size_t parsed;
	size_t remaining = num_bytes;
	char const *cursor = start_req;

	parsed = parse_post_prefix(start_req, num_bytes);
	if (parsed == 0) {
		printf("Could not parse prefix\n");
		return 0;
	}
	cursor += parsed;
	remaining -= parsed;

	parsed = parse_uri(cursor, remaining);
	if (parsed == 0) {
		printf("Could not parse uri\n");
		return 0;
	}
	cursor += parsed;
	remaining -= parsed;

	parsed = parse_http_version(cursor, remaining);
	if (parsed == 0) {
		printf("Could not parse http version\n");
		return 0;
	}
	cursor += parsed;

	return (cursor - start_req);
}

size_t parse_content_length(char const *data, size_t num_bytes) {
	char const *cursor = data;
	size_t num_bytes_content;
	static char const field_name[] = "Content-Length:";
	cursor = locate_string_bounded(data, num_bytes, field_name,
				       sizeof field_name - 1);
	if (cursor != NULL) {
		cursor += sizeof field_name - 1;
		while (*cursor == ' ')
			cursor++;
		num_bytes_content = atol(cursor);
	}
	return num_bytes_content;
}

char const *locate_string_bounded(char const *haystack, size_t nbytes_hay,
				  char const *needle, size_t nbytes_needle) {
	size_t max_offset = nbytes_hay - nbytes_needle;
	for (size_t offset = 0; offset < max_offset; offset++) {
		if (memcmp(haystack + offset, needle, nbytes_needle) == 0)
			return haystack + offset;
	}
	return NULL;
}

void append_to_file(char *filename, char *data, int size) {
	int file;
	file = open(filename, O_WRONLY | O_APPEND, S_IRUSR | S_IWUSR);
	if (file == -1) {
		fprintf(stderr, "Could not open file for writing\n");
		exit(EXIT_FAILURE);
	}
	ssize_t bytes_written = write(file, data, size);
	if (bytes_written < 0)
		fprintf(stderr, "Failed to write data");
	else {
		// Ensure data is physically committed to disk before
		// returning
		fsync(file);
		printf("score was appended to file\n");
	}
	close(file);
}
