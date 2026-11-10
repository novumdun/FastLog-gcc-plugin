read https://github.com/novumdun/FastLog-gcc-plugin/blob/main/tools/gcc_plugin.cc, I changed it.
This file is used to parse the types of args of printf.
I want to develop a new prrinf which could only save the raw data of other args except the 'format'. So I need to get the content of format at compile time. That means the I need to analyze the type of 'format' carefully.
In get_call_expression, I use 'get_type to get' the type of args for printf.
I want to divide type_pointer_char and type_string more Detailed. That means you should add some new types for string and char *. those types should help to get the content of 'format' at compile time.

# str is not changed before printf, through it's mutable
char str[]="sssss";
printf(str); 

# str is const array
const char str[]="sssss";
printf(str); 

# str is only used by p before printf, through it's mutable
char str[]="sssss";
char *p=str;
printf(p);

# str is only used by p before printf, through it's mutable
char str[]="sssss";
const char *p=str;
printf(p);

# str is const array
const char str[]="sssss";
const char *p=str;
printf(p);

# p to const array
const char *p="sssss";
printf(p);

# p to const array
const char *p="sssss";
printf(p);


printf(__func__);

printf(__FUNCTION__);

#define __LINE_STR(x) "Line" #x
#define _LINE_STR(x)  __LINE_STR(x)
#define LINE_STR      _LINE_STR(__LINE__)
printf(LINE_STR);



