#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int addr5;
int addr6;

int foo()
{
    return -1;
}
void point_at(void *p);
void foo1();
char g = 'g';
void foo2();

int secondary(int x)
{
    int addr2;
    int addr3;
    char *yos = "ree";
    int *addr4 = (int *)(malloc(50));
	int iarray[3];
    float farray[3];
    double darray[3];
    char carray[3]; 
	int iarray2[] = {1,2,3};
    char carray2[] = {'a','b','c'};
    int* iarray2Ptr;
    char* carray2Ptr; 
    
	printf("- &addr2: %p\n", &addr2);
    printf("- &addr3: %p\n", &addr3);
    printf("- foo: %p\n", &foo);
    printf("- &addr5: %p\n", &addr5);
	printf("Print distances:\n");
    point_at(&addr5);

    printf("Print more addresses:\n");
    printf("- &addr6: %p\n", &addr6);
    printf("- yos: %p\n", yos);
    printf("- gg: %p\n", &g);
    printf("- addr4: %p\n", addr4);
    printf("- &addr4: %p\n", &addr4);

    printf("- &foo1: %p\n", &foo1);
    printf("- &foo1: %p\n", &foo2);
    
    printf("Print another distance:\n");
    printf("- &foo2 - &foo1: %ld\n", (long) (&foo2 - &foo1));

   
    printf("Arrays Mem Layout (T1b):\n");

    /* task 1 b here */

    printf("address of iarray: %p\n", (void *)iarray);
    printf("address of iarray+1: %p\n", (void *)(iarray+1));
    printf("address of farray: %p\n", (void *)farray);
    printf("address of farray+1: %p\n", (void *)(farray+1));
    printf("address of darray: %p\n", (void *)darray);
    printf("address of darray+1: %p\n", (void *)(darray+1));
    printf("address of carray: %p\n", (void *)carray);
    printf("address of carray+1: %p\n", (void *)(carray+1));

    printf("Pointers and arrays (T1d): ");

    /* task 1 d here */
    iarray2Ptr = iarray2;
    carray2Ptr = carray2;
    printf("iarray2: ");
    for(int i = 0; i < 3;i++){
        printf("%d ", *(iarray2Ptr + i));
    }
    printf("\n");
    printf("carray2: ");
    for(int i = 0; i < 3;i++){
        printf("%d ", *(carray2Ptr + i));
    }
    printf("\n");

    int *p;
    printf("uninitialized pointer: %p\n", (void *)p);
}

int main(int argc, char **argv)
{ 

    printf("Print function argument addresses:\n");

    printf("- &argc %p\n", &argc);
    printf("- argv %p\n", argv);
    printf("- &argv %p\n", &argv);
	
	secondary(0);
    
    printf("Command line arg addresses (T1e):\n");
    /* task 1 e here */
    printf("address of argv: %p\n", (void *)argv);
    for(int i = 0; i < argc; i++){
        printf("address of argv[%d]: %p, content: %s\n", i, (void *)argv[i],argv[i]);
    }
    return 0;
}

void point_at(void *p)
{
    int local;
    static int addr0 = 2;
    static int addr1;

    long dist1 = (size_t)&addr6 - (size_t)p;
    long dist2 = (size_t)&local - (size_t)p;
    long dist3 = (size_t)&foo - (size_t)p;

    printf("- dist1: (size_t)&addr6 - (size_t)p: %ld\n", dist1);
    printf("- dist2: (size_t)&local - (size_t)p: %ld\n", dist2);
    printf("- dist3: (size_t)&foo - (size_t)p:  %ld\n", dist3);
    
    printf("Check long type mem size (T1a):\n");
    /* part of task 1 a here */

    printf("- addr0: %p\n", &addr0);
    printf("- addr1: %p\n", &addr1);
}

void foo1()
{
    printf("foo1\n");
}

void foo2()
{
    printf("foo2\n");
}
