#include <stdlib.h>
#include <stdio.h>
#include <string.h>
 
char* map(char *array, int array_length, char (*f) (char)){
  char* mapped_array = (char*)(malloc(array_length*sizeof(char)));
  if(mapped_array == NULL){
    return NULL;
  }
  for(int i = 0;i<array_length;i++){
    mapped_array[i] = f(array[i]);
  }
  return mapped_array;
}
/**
 * reads and returns a character from stdin using fgetc 
*/
char my_get(char c){
  char b[256];
  if (fgets(b, sizeof(b), stdin) == NULL) {
    return '\0';
  }
  return b[0];
}
/**
 * If c is a number between 0x20 and 0x7E, cprt prints the character of ASCII 
 * value c followed by a new line. Otherwise, cprt prints the dot ('.') character.
 * After printing, cprt returns the value of c unchanged. 
*/

char cprt(char c){
  if(c >= 0x20 && c <=0x7E){
    printf("%c\n", c);
  } else
  {
    printf(".\n");
  }
  return c;
}
/**
 * Gets a char c. If c is between 0x20 and 0x4E add 0x20 to its
 * value and return it. Otherwise return c unchanged
*/
char encrypt(char c){
  if(c >= 0x20 && c <=0x4E){
    return c + 0x20;
  }
  return c;
}
/**
 * Gets a char c and returns its decrypted form subtractng 0x20 from its value.
 * But if c was not between 0x40 and 0x7E it is returned unchanged.
*/
char decrypt(char c){
  if(c >= 0x40 && c <=0x7E){
    return c - 0x20;
  }
  return c;
}
/**
 * xoprt prints the value of c in a hexadecimal representation, 
 * then in octal representation, followed by a new line, and returns c unchanged.
*/
char xoprt(char c){
  printf("0x%X 0%o\n", c, c);
  return c;
}
/********************************************************/
typedef struct {
  char *n;
  char (*f)(char);
}Menu;


int main(int argc, char **argv){
  Menu menu[] ={{"Get str", my_get},{"Print str", cprt},{"Encrypt", encrypt},{"Decrypt",decrypt},{"Print Hex and Octal",xoprt},{NULL,NULL}};
  char *c = malloc(256 * sizeof(char));
  if(c == NULL){
    perror("memory don't allocated");
    return 0;
  }
  c[0] = '\0';
  while (1){
    printf("Select operation from the following menu (ctrl^D for exit):\n");
    for(int i = 0; menu[i].n != NULL; i++){
      printf("%d) %s\n", i, menu[i].n);
    }
    printf("Option: ");
    char input[10];
    if(fgets(input, sizeof(input), stdin) == NULL) {
      printf("\n");
      free(c);
      break;
    }
    int option = atoi(input);
    int menulenght = sizeof(menu) / sizeof(menu[0]) - 1;

    if (option >= 0 && option < menulenght) {
      printf("Within bounds\n");

      if (option == 0) {
        if (fgets(c, 256, stdin) == NULL) {
          printf("\n");
          free(c);
          break;
        }
        c[strcspn(c, "\n")] = '\0';
      } else {
        char *new_c = map(c, strlen(c), menu[option].f);
        if (new_c != NULL) {
          free(c);
          c = new_c;
        }
      }
      printf("DONE.\n");
    } else {
      printf("Not within bounds\n");
      free(c);
      break;
    }
  }
  
  return 0;
}
