#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
#define HT_DEBUG 0
*/

#define ASSERT_NOT_REACHED() assert(0 && "Expected this code to never run.")

/*
 * STRINGS
 *
 * Not required certainly, and probably less than memory efficient, but makes
 * writing the lexer a lot prettier, and fun is the point of this little file
 * after all, not speed.
 *
 * Note: we are only handling ascii here.  Because this is a json parsing
 * library, and unicode support, both in the string library and the lexer, is
 * too much to bite off right now.
 */
typedef struct {
	// internal repr of the string
	char* char_ptr;
	// length of the string, exluding the null terminating byte, such that
	// strlen(storage) == len
	int len;
	// memory capacity of the internal storage char_ptr
	int cap;
} string_t;

// creates a new empty string
string_t* str_alloc()
{
	string_t* s = malloc(sizeof(string_t));
	s->char_ptr = malloc(1);
	s->len = 0;
	s->cap = 1;

	return s;
}

// creates a string_t from an existing string
string_t* str_init(const char* is)
{
	int l = strlen(is);
	string_t* s = malloc(sizeof(string_t));
	s->char_ptr = malloc(l);
	strcpy(s->char_ptr, is);
	s->len = l;
	s->cap = l;

	return s;
}

void str_free(string_t *s)
{
	free(s->char_ptr);
	free(s);
}

// resizes to byte length of n (not strlen of n)
void __str_resize(string_t* s, int n) {
	s->char_ptr = realloc(s->char_ptr, n);
	s->cap = n;
}

void str_append(string_t* str, const char* a)
{
	int l = strlen(a);
	// dt = l + 1 because of the null terminating byte
	int dt = l + 1;
	if (str->cap < str->len + dt) {
		int i = str->cap * 2;
		while (i < str->len + dt) {
			i = i * 2;
		}
		__str_resize(str, i);
	}

	strcat(str->char_ptr, a);
	str->len += l;
}

// identical to str_append, except appends a single character
void str_append_char(string_t* str, const char c)
{
	// character + null terminating byte
	int l = 2;
	if (str->cap < str->len + l) {
		int i = str->cap * 2;
		while (i < str->len + l) {
			i = i * 2;
		}
		__str_resize(str, i);
	}

	str->char_ptr[str->len] = c;
	str->char_ptr[str->len + 1] = '\0';

	str->len += 1;
}


/*
 * HASHMAP
 *
 * Simple hashmap implementation using linear probing, suitable for storing
 * json objects.
 *
 * It doesn't support anything other than string keys, because JSON only allows
 * string keys. We also don't support removing keys, because we're writing a
 * parser -- we're never going to actually manipulate the data here.
 */

typedef struct ht_entry {
	const char* key;
	void* value;
	struct ht_entry* next;
} ht_entry_t;

typedef struct {
	int cap;
	float load_factor;
	int len;
	ht_entry_t **buckets;
} hash_map_t;

// allocates a hash map with a specific initial size
hash_map_t* hash_map_alloc(int is)
{
	hash_map_t* ht = malloc(sizeof(hash_map_t));
	ht->len = 0;
	ht->cap = is;
	ht->load_factor = 0.75;
	ht->buckets = calloc(is, sizeof(ht_entry_t*));

	return ht;
}

// frees the hash map, and all the entries.  frees the entry values as well, if
// free_values = 1, and not if free_values = 0;
void hash_map_free(hash_map_t* ht, int free_values)
{
	for (int i = 0; i < ht->cap; i++) {
		ht_entry_t* curr = ht->buckets[i];
		while (curr != NULL) {
			ht_entry_t* temp = curr->next;
			if (free_values != 0) {
				free(curr->value);
			}
			free(curr);
			curr = temp;
		}
	}
	free(ht);
}

typedef struct {
	int curr_bucket;
	int curr_idx;
} ht_iterator_t;

ht_iterator_t* ht_get_iter()
{
	ht_iterator_t *it = malloc(sizeof(ht_iterator_t));
	it->curr_bucket = 0;
	it->curr_idx = 0;
	return it;
}

void ht_iter_done(ht_iterator_t *it)
{
	free(it);
}

ht_entry_t* ht_get_next(hash_map_t* ht, ht_iterator_t* it)
{
	if (it->curr_idx != 0) {
		// we're iterating down a LL
		ht_entry_t *bucket = ht->buckets[it->curr_bucket];
		for (int i = 0; i < it->curr_idx; i++) {
			bucket = bucket->next;
		}
		if (bucket->next != NULL) {
			// there's still more to do in this LL, so keep the iterator on
			// this bucket, and just increment to the next item in the LL
			it->curr_idx++;
		} else {
			// if we've reached the end of the LL, signal to the next iteration
			// that we're done checking the LL and need to move on to the next
			// iteration
			it->curr_idx = 0;
			it->curr_bucket++;
		}

		return bucket;
	}

	// find a bucket that has something in it
	ht_entry_t *bucket = ht->buckets[it->curr_bucket];
	while (bucket == NULL && it->curr_bucket < ht->cap) {
		it->curr_bucket++;
		bucket = ht->buckets[it->curr_bucket];
	}

	if (bucket == NULL) {
		return NULL;
	}

	// when we find a bucket with an entry, return it, and if there's an entry
	// in the linked list, increment the LL iterator
	if (bucket->next != NULL) {
		it->curr_idx++;
	} else {
		it->curr_bucket++;
	}

	return bucket;
}

// re-implementation of java's str hashcode algorithm
size_t str_hash(const char* str)
{
	int l = strlen(str);
	size_t hash = 7;
	for (int i = 0; i < l; i++) {
		hash = hash * 31 + str[i];
	}

	return hash;
}

ht_entry_t* __ht_get_impl(hash_map_t* ht, const char* key, size_t hash)
{
	ht_entry_t* entry = ht->buckets[hash % ht->cap];
	while (entry != NULL && strcmp(key, entry->key) != 0) {
		entry = entry->next;
	}

	return entry;
}

void* ht_get(hash_map_t* ht, const char* key) {
	size_t hash = str_hash(key);
	ht_entry_t* e = __ht_get_impl(ht, key, hash);
	if (e != NULL) {
		return e->value;
	}

	return NULL;
}

void __ht_buckets_set(ht_entry_t** buckets, ht_entry_t* new_entry, const char* key, size_t hash, int cap)
{
	ht_entry_t* ee = buckets[hash % cap];
#ifdef HT_DEBUG
	printf("__ht_buckets_set: (cap: %d) placing new entry (%s) into bucket: %lu\n", cap, key, hash % cap);
#endif

	// if no existing entry, set and stop quickly
	if (ee == NULL) {
		buckets[hash % cap] = new_entry;
		return;
	}

	// if there is an existing entry, while said entry's key is not equal to
	// the key we're inserting (double-entry = NOOP), forward ee to the next
	// value on the linked list.
	while (ee->next != NULL && strcmp(ee->key, key) != 0) {
		ee = ee->next;
	}

	if (strcmp(ee->key, key) != 0) {
		// if the existing entry's key is not the same as the key to insert, insert the new entry
		ee->next = new_entry;
		return;
	}

	// fallthrough: if we're here, then we've gotten two set ops for the same key, so do nothing
#ifdef HT_DEBUG
	printf("__ht_buckets_set: duplicate entries attempted insert with key %s\n", key);
#endif
}

// grows and rebalances the hash table
void __ht_grow(hash_map_t *ht)
{
	int ns = ht->cap * 2;
#ifdef HT_DEBUG
	printf("__ht_grow: resizing hash table from %d to %d\n", ht->cap, ns);
#endif
	ht_entry_t **buckets = calloc(ns, sizeof(ht_entry_t*));

	// rebalance with new buckets
	for (int i = 0; i < ht->cap; i++) {
		ht_entry_t *e = ht->buckets[i];
		while (e != NULL) {
			ht_entry_t* n = e->next;
			e->next = NULL;
			__ht_buckets_set(buckets, e, e->key, str_hash(e->key), ns);
			e = n;
		}
	}

	free(ht->buckets);
	ht->buckets = buckets;
	ht->cap = ns;
}

void ht_set(hash_map_t *ht, const char* key, void* value)
{
	if (((float)ht->len + 1.0) / ((float)ht->cap) >= ht->load_factor) {
		// resize first
		__ht_grow(ht);
	}

	size_t hash = str_hash(key);
	ht_entry_t* new_entry = malloc(sizeof(ht_entry_t));
	new_entry->value = value;
	new_entry->key = key;
	new_entry->next = NULL;

	__ht_buckets_set(ht->buckets, new_entry, key, hash, ht->cap);
	ht->len++;
}

/*
 * LEXER
 */

typedef enum {
	TOKEN_EOF,
	BRACKET_OPEN,
	BRACKET_CLOSE,
	ARRAY_OPEN,
	ARRAY_CLOSE,
	COLON,
	STRING,
	COMMA,
	KEYWORD,
	NUMBER,
} token_type_t;

const char* token_type_repr(token_type_t type)
{
	switch (type) {
		case TOKEN_EOF:
			return "EOF";
		case BRACKET_OPEN:
			return "BRACKET_OPEN";
		case BRACKET_CLOSE:
			return "BRACKET_CLOSE";
		case ARRAY_OPEN:
			return "ARRAY_OPEN";
		case ARRAY_CLOSE:
			return "ARRAY_CLOSE";
		case COLON:
			return "COLON";
		case COMMA:
			return "COMMA";
		case STRING:
			return "STRING";
		case NUMBER:
			return "NUMBER";
		case KEYWORD:
			return "KEYWORD";
		default:
			return "UKNOWN_TYPE";
	}
}

typedef struct {
	string_t *lexeme;
	token_type_t type;
} token_t;

// allocates a token that is identical to its type, for example, BRACKET_OPEN
// saves a string allocation where we don't actually care about the lexeme
token_t* single_type_token(token_type_t tp)
{
	token_t* t = malloc(sizeof(token_t));
	t->type = tp;
	t->lexeme = NULL;
	return t;
}

token_t* token_alloc()
{
	token_t* t = malloc(sizeof(token_t));
	t->lexeme = str_alloc();
	return t;
}

void token_free(token_t *t)
{
	str_free(t->lexeme);
	free(t);
}

char* token_repr(token_t* token)
{
	string_t* fmt = str_alloc();
	str_append(fmt, "Token { type: ");
	str_append(fmt, token_type_repr(token->type));
	if (token->lexeme != NULL) {
		str_append(fmt, ", lexeme: ");
		str_append(fmt, token->lexeme->char_ptr);
	}
	str_append(fmt, " }");
	char* r = fmt->char_ptr;
	free(fmt);
	return r;
}

token_t* get_next_token(FILE *stream)
{
	char c = getchar();

	// skip whitespace
	while (isspace(c) && (c = getchar()) != EOF) {}

	if (c == EOF) {
		return single_type_token(TOKEN_EOF);
	} else if (c == '{') {
		return single_type_token(BRACKET_OPEN);
	} else if (c == '}') {
		return single_type_token(BRACKET_CLOSE);
	} else if (c == ':') {
		return single_type_token(COLON);
	} else if (c == ',') {
		return single_type_token(COMMA);
	} else if (c == '[') {
		return single_type_token(ARRAY_OPEN);
	} else if (c == ']') {
		return single_type_token(ARRAY_CLOSE);
	} else if (c == '"') {
		token_t *t = token_alloc();
		t->type = STRING;
		while ((c = getchar()) != EOF && c != '"') {
			if (c == '\\') {
				// ignore both this character and the next one
				c = getchar();
			}
			str_append_char(t->lexeme, c);
		}
		if (c == EOF) {
			// put EOF back on the stream so we can get it as a token later...
			ungetc(c, stream);
		}
		// TODO: don't allow strings with form-feeds, carriage returns, or newlines
		return t;
	} else if (isdigit(c)) {
		token_t *t = token_alloc();
		t->type = NUMBER;
		str_append_char(t->lexeme, c);
		while ((c = getchar()) != EOF && isdigit(c)) {
			str_append_char(t->lexeme, c);
		}
		ungetc(c, stream);
		return t;
	} else if (isalpha(c)) {
		token_t *t = token_alloc();
		t->type = KEYWORD;
		str_append_char(t->lexeme, c);
		while ((c = getchar()) != EOF && isalpha(c)) {
			str_append_char(t->lexeme, c);
		}
		ungetc(c, stream);
		return t;
	} else {
		printf("Unexpected character %c.  Failed to parse.\n", c);
		exit(1);
	}
}

// See https://www.json.org/json-en.html
typedef enum {
	JSON_OBJECT,
	JSON_ARRAY,
	JSON_STRING,
	JSON_NUMBER,
	JSON_TRUE,
	JSON_FALSE,
	JSON_NULL,
} json_type_t;


struct json_value;

typedef struct json_value {
	json_type_t type;
	union {
		hash_map_t* map;
		struct {
			struct json_value **array;
			int cap;
			int len;
		} as_array;
		string_t* string;
		int boolean;
		int number;
	} value;
} json_value_t;

// internal method for json_value_t that supports resizing the array value
void __json_value_array_resize(json_value_t *j, int ns)
{
	printf("trying to allocate new array of size %d\n", ns);
	// unsafe: use reallocarray instead
	j->value.as_array.array = realloc(j->value.as_array.array, sizeof(json_value_t*) * ns);
	j->value.as_array.cap = ns;
}

// internal method for appending new values to the underlying array
void __json_value_array_append(json_value_t *parent, json_value_t *new_item)
{
	int new_cap = parent->value.as_array.len + 1;
	if (parent->value.as_array.cap < new_cap) {
		int resize = parent->value.as_array.cap * 2;
		__json_value_array_resize(parent, resize);
	}

	parent->value.as_array.array[new_cap - 1] = new_item;
	parent->value.as_array.len++;
}

// creates a new value with an uninitialized "value" type
json_value_t* new_value(json_type_t type) {
	json_value_t *v = malloc(sizeof(json_value_t));
	v->type = type;
	return v;
}


/*
 * Simple recursive descent parser
 */

typedef struct {
	token_t* lookahead;
	FILE *stream;
} parser_t;

// forward declare...
json_value_t* parse_value(parser_t *parser);

void panic_unexpected_token(token_t *token)
{
	printf("Unexpected token %s.\n", token_repr(token));
	exit(1);
}

void match(parser_t* parser, token_type_t type)
{
	if (parser->lookahead->type == type) {
		parser->lookahead = get_next_token(parser->stream);
	} else {
		panic_unexpected_token(parser->lookahead);
	}
}

parser_t* new_parser(FILE *stream)
{
	parser_t* p = malloc(sizeof(parser_t));
	p->stream = stream;
	p->lookahead = get_next_token(stream);
	return p;
}

json_value_t* parse_keyword(token_t* t)
{
	json_value_t *ret;

	if (t->lexeme == NULL) {
		printf("Internal Error: parser_keyword_to_type: expected token with lexeme");
		exit(1);
	}

	ret = malloc(sizeof(*ret));

	if (strcmp(t->lexeme->char_ptr, "true") == 0) {
		ret->type = JSON_TRUE;
		ret->value.boolean = 1;
	} else if (strcmp(t->lexeme->char_ptr, "false") == 0) {
		ret->type = JSON_FALSE;
		ret->value.boolean = 0;
	} else if (strcmp(t->lexeme->char_ptr, "null") == 0) {
		ret->type = JSON_NULL;
	} else {
		printf("Invalid keyword '%s'\n", t->lexeme->char_ptr);
		exit(1);
	}

	return ret;
}

// parent = object
void parse_single_object_member(parser_t *parser, json_value_t *parent)
{
	if (parser->lookahead->type != STRING) {
		panic_unexpected_token(parser->lookahead);
	}

	char* key = parser->lookahead->lexeme->char_ptr;
	match(parser, STRING);
	match(parser, COLON);

	json_value_t* value = parse_value(parser);

	ht_set(parent->value.map, key, value);
}

// parent = object
void parse_rest_object_members(parser_t* parser, json_value_t *parent)
{
	if (parser->lookahead->type == COMMA) {
		match(parser, COMMA);
		parse_single_object_member(parser, parent);
		parse_rest_object_members(parser, parent);
	}
}

// parent = object
void parse_json_object_members(parser_t *parser, json_value_t* parent)
{
	parse_single_object_member(parser, parent);
	parse_rest_object_members(parser, parent);
}

json_value_t* parse_json_object(parser_t *parser)
{
	// parent could only be an array
	match(parser, BRACKET_OPEN);
	json_value_t* ret = new_value(JSON_OBJECT);
	// TODO: figure out correct starting size
	ret->value.map = hash_map_alloc(4);
	if (parser->lookahead->type == BRACKET_CLOSE) {
		// we have an empty object
		match(parser, BRACKET_CLOSE);
		return ret;
	}

	parse_json_object_members(parser, ret);
	match(parser, BRACKET_CLOSE);

	return ret;
}

void parse_json_array_members(parser_t* parser, json_value_t *parent)
{
	// we can safely assume that there will always be at least one member in
	// here...
	json_value_t *value = parse_value(parser);
	__json_value_array_append(parent, value);
	if (parser->lookahead->type == COMMA) {
		match(parser, COMMA);
		parse_json_array_members(parser, parent);
	}
}

json_value_t *parse_json_array(parser_t *parser)
{
	match(parser, ARRAY_OPEN);
	json_value_t *ret = new_value(JSON_ARRAY);

	int initial_array_size = 2;

	json_value_t **new_array = calloc(sizeof(json_value_t*), initial_array_size);
	// it's important that we allocate an empty array here, otherwise later
	// code will segfault when trying to get the length of the array (0).
	// Maybe we could optimize this eventually.
	ret->value.as_array.len = 0;
	ret->value.as_array.cap = initial_array_size;
	ret->value.as_array.array = new_array;

	if (parser->lookahead->type == ARRAY_CLOSE) {
		// empty array
		match(parser, ARRAY_CLOSE);
		return ret;
	}

	parse_json_array_members(parser, ret);
	match(parser, ARRAY_CLOSE);

	return ret;
}

json_value_t* parse_value(parser_t *parser)
{
	// parse the possible starting points
	if (parser->lookahead->type == BRACKET_OPEN) {
		// parse an object
		json_value_t *v = parse_json_object(parser);
		return v;
	} else if (parser->lookahead->type == KEYWORD) {
		// special case: a keyword cannot have children
		json_value_t *v = parse_keyword(parser->lookahead);
		// because this is terminal, we must advance the token ourselves
		match(parser, KEYWORD);
		return v;
	} else if(parser->lookahead->type == NUMBER) {
		// special case: a number cannot have children
		json_value_t *v = new_value(JSON_NUMBER);
		v->value.number = atoi(parser->lookahead->lexeme->char_ptr);
		match(parser, NUMBER);
		return v;
	} else if (parser->lookahead->type == STRING) {
		// special case: a string cannot have children
		json_value_t *v = new_value(JSON_STRING);
		v->value.string = parser->lookahead->lexeme;
		match(parser, STRING);
		return v;
	} else if (parser->lookahead->type == ARRAY_OPEN) {
		json_value_t *v = parse_json_array(parser);
		return v;
	} else {
		panic_unexpected_token(parser->lookahead);
	}

	ASSERT_NOT_REACHED();
}

void print_json_value(json_value_t* tree)
{
	switch(tree->type) {
		case JSON_NUMBER:
			printf("%d", tree->value.number);
			break;
		case JSON_STRING:
			printf("\"%s\"", tree->value.string->char_ptr);
			break;
		case JSON_TRUE:
			printf("true");
			break;
		case JSON_FALSE:
			printf("false");
			break;
		case JSON_NULL:
			printf("null");
			break;
		case JSON_OBJECT: {
			hash_map_t *map = tree->value.map;
			ht_iterator_t* it = ht_get_iter();
			ht_entry_t* n;
			printf("{ ");
			int i = 0;
			while ((n = ht_get_next(map, it)) != NULL) {
				printf("\"%s\": ", n->key);
				json_value_t *value = (json_value_t*)n->value;
				print_json_value(value);
				i++;
				if (i != map->len) {
					printf(",");
				}
			}
			ht_iter_done(it);
			printf(" }");
			break;
	    }
		case JSON_ARRAY: {
			printf("[ ");
			for (int i = 0; i < tree->value.as_array.len; i++) {
				print_json_value(tree->value.as_array.array[i]);
				if (i != tree->value.as_array.len - 1) {
					printf(", ");
				}
			}
			printf(" ]");
		}
	}
}

// TODO: accept some kind of configuration for pretty printing
void pretty_print_parse_tree(json_value_t* tree)
{
	print_json_value(tree);
	printf("\n");
}

// TODO:
// - validate strings better
// - "handle" floating point numbers (aaaahhh)
// - make the parser do something actually useful, instead of just parsing and
// then spitting it back out?
int main(void)
{
	parser_t *p = new_parser(stdin);
	json_value_t *value = parse_value(p);
	pretty_print_parse_tree(value);
}
