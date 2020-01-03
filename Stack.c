#include <stdio.h>
#include <stdlib.h>

int top;
int *  stack;

#define STACK_SIZE  10
void Stack(){
	top = -1;
	stack = (int *) malloc(sizeof(int) * STACK_SIZE);
}

int peek(){
	return stack[top];
}

void push(int value){
	stack[++top] = value;
	printf(stack[top] + " PUSH !\n");
}

int pop(){
	printf(stack[top] + " POP !\n");
	return stack[top--];
}

void printStack(){
	printf("\n-----STACK LIST-----\n");

	for(int i=top; i>=0; i--){
		printf("%d\n",stack[i]);
	}

	printf("-----END OF LIST-----\n");
}
