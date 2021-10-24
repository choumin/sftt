/*
 * Copyright (C)  2020-2021 Min Zhou <zhoumin@bupt.cn>, all rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define DEBUG 0

#define SZ_1MB	(1024 * 1024)

struct st_list {
	char struct_name[1024];
	struct st_list *next;
};

static size_t file_size(char *filename) {
	FILE *pfile = fopen(filename, "rb");
	if (pfile == NULL)
	{
		return 0;
	}

	fseek(pfile, 0, SEEK_END);
	size_t length = ftell(pfile);
	rewind(pfile);

	fclose(pfile);

	return length;
}

static bool endswith(char *src, char *foo)
{
	int src_len = 0, foo_len = 0;
	char *pos = NULL;

	if (src == NULL || foo == NULL)
		return false;

	src_len = strlen(src);
	foo_len = strlen(foo);
	if (src_len < foo_len)
		return false;

	if ((pos = strstr(src, foo)) == NULL)
		return false;

	return strlen(pos) == foo_len;
}

char *fetch_next_str(char **str)
{
	char *p, *q;

	if (str == NULL || *str == NULL)
		return NULL;

	p = *str;
	/* skip blank */
	while (*p && isspace(*p))
		++p;
	if (!*p)
		return NULL;

	q = p;
	while (*q && !isspace(*q))
		++q;
	if (*q) {
		*q = 0;
		*str = q + 1;
	} else {
		*str = q;
	}

	return p;
}

int req_resp_filter(char *struct_name)
{
	if (endswith(struct_name, "_resp")) {
		printf("%s:%d, strct_name=%s\n", __func__, __LINE__, struct_name);
		return 0;
	}

	return 1;
}

int get_struct_list(char *xdr_file, struct st_list **phead)
{
	FILE *xdr_fp = NULL;
	char *content = NULL;
	char *p = NULL;
	char *q = NULL;
	char *key_word = NULL;
	char *struct_name = NULL;
	char *word = NULL;
	size_t size = 0;
	int ret = 0, len = 0, idx = 0;
	struct st_list *head = NULL, *node = NULL, *tail = NULL;

	if ((size = file_size(xdr_file)) > SZ_1MB) {
		printf("file %s too long, failed to hanlde\n", xdr_file);
		return -1;
	}
	//printf("xdr file size: %d\n", size);

	content = malloc(size + 1);
	if (!content) {
		printf("cannot alloc memory for xdr file content\n");
		return -1;
	}
	memset(content, 0, size + 1);

	xdr_fp = fopen(xdr_file, "r");
	assert(xdr_fp != NULL);

	ret = fread(content, 1, size, xdr_fp);
	assert(ret == size);
	fclose(xdr_fp);

	//printf("begin parse xdr file\n");
	p = content;
	key_word = fetch_next_str(&p);
#if DEBUG
	printf("key word: %s\n", key_word);
#endif
	while (key_word && strlen(key_word)) {
		if (strcmp(key_word, "struct") == 0) {
			struct_name = fetch_next_str(&p);
			if (struct_name == NULL)
				break;

			len = strlen(struct_name);
			if (len == 0)
				break;

			q = strchr(struct_name, '{');
			if (q) {
				*q = 0;
				assert(strlen(struct_name) < 1024);
				if (req_resp_filter(struct_name))
					goto fetch_next;
				//printf("find a struct: %s\n", struct_name);
				node = malloc(sizeof(struct st_list));
				assert(node != NULL);

				strcpy(node->struct_name, struct_name);
				node->next = NULL;

				if (head == NULL) {
					head = tail = node;
				} else {
					tail->next = node;
					tail = node;
				}

			} else {
				word = fetch_next_str(&p);
				if (word == NULL)
					break;

				if (word[0] == '{') {
					assert(strlen(struct_name) < 1024);
					if (req_resp_filter(struct_name))
						goto fetch_next;
					//printf("find a struct: %s\n", struct_name);

					node = malloc(sizeof(struct st_list));
					assert(node != NULL);

					strcpy(node->struct_name, struct_name);
					node->next = NULL;

					if (head == NULL) {
						head = tail = node;
					} else {
						tail->next = node;
						tail = node;
					}
				}
			}
		}
fetch_next:
		key_word = fetch_next_str(&p);
#if DEBUG
		if (key_word)
			printf("key word: %s\n", key_word);
#endif
	}

	//printf("end parse xdr file\n");
	free(content);

	*phead = head;

	return 0;
}

void show_st_list(struct st_list *head)
{
	struct st_list *p = head;

	printf("struct name list: ");
	while (p) {
		printf("%s ", p->struct_name);
		p = p->next;
	}
	printf("\n");
}

int gen_h_file(struct st_list *head, char *h_file)
{
	struct st_list *p = head;
	FILE *fp = NULL;
	assert(head != NULL && h_file != NULL);

	fp = fopen(h_file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open '%s'\n", h_file);
		return -1;
	}

	fprintf(fp, "/*\n * Automatically generated - do not edit\n */\n\n");
	fprintf(fp, "#ifndef _TRANS_H_\n");
	fprintf(fp, "#define _TRANS_H_\n\n");

	fprintf(fp, "#include \"packet.h\"\n\n");
	while (p) {
		fprintf(fp, "int send_%s(int fd, struct sftt_packet *resp_packet,\n"
			"\tstruct %s *resp, int code, int next);\n\n",
			p->struct_name, p->struct_name);
		p = p->next;
	}
	fprintf(fp, "#endif\n");

	fclose(fp);

	return 0;
}

char *get_packet_block_size(char *block_size, char *name)
{
	int i;

	for (i = 0; name[i]; ++i) {
		block_size[i] = toupper(name[i]);
	}
	block_size[i] = 0;
	strcat(block_size, "_PACKET_MIN_LEN");

	return block_size;
}

char *get_packet_type(char *packet_type, char *name)
{
	int i, j, count;
	
	count = sprintf(packet_type, "PACKET_TYPE_");
	for (i = count, j = 0; name[j]; ++j, ++i) {
		packet_type[i] = toupper(name[j]);
	}
	packet_type[i] = 0;

	return packet_type;
}

void output_send_resp(FILE *fp, char *struct_name)
{
	char packet_type[128];
	char block_size[128];

	fprintf(fp, "int send_%s(int fd, struct sftt_packet *resp_packet,\n"
		"\tstruct %s *resp, int code, int next)\n", struct_name,
		struct_name);
	fprintf(fp, "{\n");
	fprintf(fp, "\tresp->status = code;\n");
	fprintf(fp, "\tstrncpy(resp->message, resp_messages[code],"
		" RESP_MESSAGE_MAX_LEN - 1);\n");
	fprintf(fp, "\tresp->next = next;\n\n");
	fprintf(fp, "\tresp_packet->obj = resp;\n");
	fprintf(fp, "\tresp_packet->type = %s;\n",
		get_packet_type(packet_type, struct_name));
	fprintf(fp, "\tresp_packet->block_size = %s;\n\n",
		get_packet_block_size(block_size, struct_name));

	fprintf(fp, "\treturn send_sftt_packet(fd, resp_packet);\n");
	fprintf(fp, "}\n");
}

int gen_c_file(struct st_list *head, char *c_file)
{
	FILE *fp = NULL;
	struct st_list *p = head;
	assert(head != NULL && c_file != NULL);

	fp = fopen(c_file, "w");
	if (fp == NULL) {
		fprintf(stderr, "Failed to open '%s'\n", c_file);
		return -1;
	}

	fprintf(fp, "/*\n * Automatically generated - do not edit\n */\n\n");
	fprintf(fp, "#include \"net_trans.h\"\n");
	fprintf(fp, "#include \"req_resp.h\"\n");
	fprintf(fp, "#include \"response.h\"\n");
	fprintf(fp, "#include \"trans.h\"\n\n");
	fprintf(fp, "extern const char *resp_messages[];\n\n");

	while (p) {
		output_send_resp(fp, p->struct_name);
		p = p->next;
		if (p) {
			fprintf(fp, "\n");
		}
	}

	fclose(fp);

	return 0;
}

int gen_trans(struct st_list *head, char *h_file, char *c_file)
{
	FILE *fp = NULL;

	if (gen_h_file(head, h_file) == -1) {
		printf("gen header file failed!\n");
		return -1;
	};

	if (gen_c_file(head, c_file) == -1) {
		printf("gen src file failed!\n");
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct st_list *head = NULL;
	if (argc != 4) {
		printf("Usage: %s xdr_file h_file c_file\n", argv[0]);
		return -1;
	}

	if (get_struct_list(argv[1], &head) == -1) {
		printf("get struct list failed\n");
		return -1;
	}

	show_st_list(head);
	if (gen_trans(head, argv[2], argv[3]) == -1) {
		printf("gen trans failed\n");
		return -1;
	}

	return 0;
}
