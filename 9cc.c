#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "error.c"

enum {
  TK_NUM = 256,
  TK_EOF,
  ND_NUM = 256,
  TK_EQ,
  TK_NE,
  TK_LE,
  TK_GE,
};

typedef struct {
  int ty;
  int val;
  char *input;
} Token;

typedef struct Node {
  int ty;
  struct Node *lhs;
  struct Node *rhs;
  int val;
} Node;

typedef struct Vector {
  void **data;
  int capacity;
  int len;
} Vector;

Vector *new_vector() {
  Vector *vec = malloc(sizeof(Vector));
  vec->data = malloc(sizeof(void*)*16);
  vec->capacity = 16;
  vec->len = 0;
  return vec;
}

Vector *vec_push(Vector *vec, void *elem){
  if(vec->capacity == vec->len){
    vec->capacity *= 2;
    vec->data = realloc(vec->data, sizeof(void *) * vec->capacity);
  }
  vec->data[vec->len++] = elem;
}

Token tokens[100];
int pos;
Node *add();
Node *mul();
Node *unary();
Node *term();

Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

int consume(int ty) {
  if (tokens[pos].ty != ty){
    return 0;
  }
  pos++;
  return 1;
}

Node *equality(){
  Node *node = add();
  for(;;){
    if(consume(TK_EQ))
      node = new_node(TK_EQ, node, add());
    else if(consume(TK_NE))
      node = new_node(TK_NE, node, add());
    else
      return node;
  }

}

Node *add(){
  Node *node = mul();

  for (;;) {
    if (consume('+'))
      node = new_node('+', node, mul());
    else if (consume('-'))
      node = new_node('-', node, mul());
    else
      return node;
  }
}

Node *mul(){
  Node *node = unary();

  for(;;){
    if(consume('*'))
      node = new_node('*', node, unary());
    else if(consume('/'))
      node = new_node('/', node, unary());
    else
    return node;
  }
}

Node *unary(){
  for(;;){
    if(consume('+'))
      return term();
    else if(consume('-'))
      return new_node('-', new_node_num(0), term());
    return term();
  }
}

Node *term(){
  if(consume('(')){
    Node *node = add();
    if(!consume(')'))
      error("開きカッコに対応する閉じカッコがありません: %s", tokens[pos].input);
    return node;
  }

  if(tokens[pos].ty == TK_NUM)
    return new_node_num(tokens[pos++].val);

  error("数値でも開きカッコでもないトークンです: %s", tokens[pos].input);
}

void tokenize(char *p) {
  int i = 0;
  while (*p) {
    if (isspace(*p)) {
      p++;
      continue;
    }

    if(*p=='='){
      if(*p++=='='){
        tokens[i].ty = TK_EQ;
        p++;
        tokens[i].input = p;
        i++;
        continue;
      }
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '(' || *p == ')') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);
      i++;
      continue;
    }

    error("トークナイズできません: %s", p);
    exit(1);
  }

  tokens[i].ty = TK_EOF;
  tokens[i].input = p;
}

void gen(Node *node) {
  if (node->ty == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->ty) {
  case TK_EQ:
    printf("  cmp rax, rdi\n");
    printf("  sete al\n");
    printf("  movzx rax, al\n");
  case TK_NE:
    printf("  cmp rax, rdi\n");
    printf("  sete al");
    printf("  movzx rax, al");
  case '+':
    printf("  add rax, rdi\n");
    break;
  case '-':
    printf("  sub rax, rdi\n");
    break;
  case '*':
    printf("  mul rdi\n");
    break;
  case '/':
    printf("  mov rdx, 0\n");
    printf("  div rdi\n");
  }

  printf("  push rax\n");
}

int expect(int line, int expected, int actual) {
  if (expected == actual)
    return 0;
  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);
  exit(1);
}

void runtest() {
  Vector *vec = new_vector();
  expect(__LINE__, 0, vec->len);

  for (int i = 0; i < 100; i++)
    vec_push(vec, (void *)i);

  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, (long)vec->data[0]);
  expect(__LINE__, 50, (long)vec->data[50]);
  expect(__LINE__, 99, (long)vec->data[99]);

  printf("OK\n");
}

int main(int argc, char **argv) {
  if(strcmp(argv[1], "-test")==0){
    runtest();
  }else{
    if (argc != 2) {
      fprintf(stderr, "引数の個数が正しくありません\n");
      return 1;
    }

    tokenize(argv[1]);
    int i;
    for(i=0;i<4;i++){
      printf("%s\n", "----");
      printf("%d\n", tokens[i].ty);
      printf("%s\n", tokens[i].input);
    }
    Node *node = equality();

    printf(".intel_syntax noprefix\n");
    printf(".global _main\n");
    printf("_main:\n");

    gen(node);

    printf("  pop rax\n");
    printf("  ret\n");
  }
  return 0;
}
