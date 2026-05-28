void terminal_write(const char *str, int len) {
    for (int i = 0; i < len; i++) {
        *(char*)(0x10000000) = str[i]; // writing to this address, will print a character on terminal
    }
}

/* Uncomment line11 - line39
 * when implementing formatted output
 */

#include <stdlib.h>  // for itoa() and utoa()
#include <string.h>  // for strlen() and strcat()
#include <stdarg.h>  // for va_start(), va_end(), va_arg() and va_copy()
// va means variable argument
/*
modify format to str, so the following compile:
printf("%c is character $", '$');
printf("%c is character 0", (char)48);
printf("%x is integer 1234 in hexadecimal", 1234);
printf("%u is the maximum of unsigned int", (unsigned int)0xFFFFFFFF);
printf("%p is the hexadecimal address of the hello-world string", msg);
printf("%llu is the maximum of unsigned long long", 0xFFFFFFFFFFFFFFFFULL);

*/
void format_to_str(char* out, const char* fmt, va_list args) {
    // fmt is the format of the string input
    // out is where output is written to, in the output buffer
    for(out[0] = 0; *fmt != '\0'; fmt++) { // while we are not at last character
        if (*fmt != '%') {
            strncat(out, fmt, 1);
        } else {
            fmt++;
            switch (*fmt) {
                case 's':
                    strcat(out, va_arg(args, char*)); //va_arg(args, Type of next parameter)
                    break;
                case 'd':
                    // integer to string; integer, memory location; initial pointer of out, then string length, base
                    itoa(va_arg(args, int), out + strlen(out), 10);
                    break;
                case 'c':
                    char c = va_arg(args,int);
                    strcat(out, &c);
                    break;
                case 'x':
                    itoa(va_arg(args,int), out + strlen(out), 16);
                    break;
                
                case 'p':
                    char* msg = va_arg(args, char*); // pointer, need to cast to int for itoa
                    strcat(out, "0x");
                    itoa((int)msg, out + strlen(out), 16);
                    break;
                
                case 'u':{
                    const size_t INT_LEN = 21;
                    char uint_arr[INT_LEN];
                    uint_arr[INT_LEN-1] = '\0';
                    int ind = INT_LEN-2;

                    unsigned int res = va_arg(args, unsigned int); // write digit by digit                    
                    while(res > 0){ 
                        uint_arr[ind] = (res%10) + '0';
                        ind--;
                        res/=10;
                    }
                    strcat(out, &uint_arr[ind+1]);
                    break;
                }
                case 'l':{
                    const size_t INT_LEN = 21;
                    char uint_arr[INT_LEN];
                    uint_arr[INT_LEN-1] = '\0';
                    int ind = INT_LEN-2;
                    const char* lu = fmt;
                    lu++;
                    if(*lu == 'l' && *(lu+1) == 'u'){
                        unsigned long long res = va_arg(args, unsigned long long);       
                        while(res > 0){ 
                            uint_arr[ind] = (res%10) + '0';
                            ind--;
                            res/=10;
                        }
                        strcat(out, &uint_arr[ind+1]);
                        fmt += 2;
                        break;
                    }
                    break;
                }
            }
        }
    }
}

unsigned int format_to_str_len(const char* fmt, va_list args){
    // want to calculate resulting string length
    unsigned int len = 0;
    for(; *fmt != '\0'; fmt++) { // while we are not at last character
        if (*fmt != '%') {
            len++;
        } else {
            fmt++;
            switch (*fmt) {
                case 's':{
                    len += strlen(va_arg(args, char*));
                    break;
                }
                case 'd':{
                    int res = va_arg(args, int);
                    while (res > 0){
                        len++;
                        res/=10;
                    }
                    break;
                }
                case 'c':{
                    char c = va_arg(args,int); //take int, cast to char
                    len ++;
                    break;
                }
                case 'x':{ // hexadecimal
                    int res = va_arg(args, int);
                    while (res > 0){
                        len++;
                        res/=16;
                    }
                    break;
                }
                case 'p':{
                    char* msg = va_arg(args, char*); // pointer, need to cast to int for itoa
                    len += 2; // for 0x
                    int address = (int)msg;
                    while(address > 0){
                        len ++;
                        address /= 16;
                    }
                    break;
                }
                
                case 'u':{
                    unsigned int res = va_arg(args, unsigned int); // write digit by digit                    
                    while(res > 0){ 
                        len++;
                        res/=10;
                    }
                    break;
                }
                case 'l':{
                    const char* lu = fmt;
                    lu++;
                    if(*lu == 'l' && *(lu+1) == 'u'){
                        unsigned long long res = va_arg(args, unsigned long long);       
                        while(res > 0){ 
                            len++;
                            res/=10;
                        }
                        fmt += 2;
                        break;
                    }
                    break;
                }
            }
        }
    }
    return len;
}
int printf(const char* format, ...) {
    // dynamic memory allocation; during compile time, length of output string can be unknown
    va_list args;
    va_start(args, format);

    va_list args_copy;
    va_copy(args_copy, args);
    unsigned int len = format_to_str_len(format, args_copy);
    char *buf = malloc(len);
    format_to_str(buf, format, args);
    va_end(args);
    terminal_write(buf, strlen(buf));

    va_end(args_copy);
    free(buf);
    return 0;
}


/* Uncomment line46 - line57
 * when implementing dynamic memory allocation, oops LOOL
 */
/*
what is this for?
used to hardcode where in memory heap_start and end are, since we are working on bare metal riscv (ie no OS underneath)
    this is to ensure heap and stack dont overlap

Break Pointer: used to make sure heap and stack dont overlap
    _sbrk is called by malloc (eventually), but since on baremetal, not provided


__heap_start and __heap_end define the allowed heap region in memory.
_sbrk manages the current used portion of that region.
- _sbrk enforces the boundaries, and grows the heap within them

*/
extern char __heap_start, __heap_end;
static char* brk = &__heap_start;
char* _sbrk(int size) {
    if (brk + size > (char*)&__heap_end) {
        terminal_write("_sbrk: heap grows too large\r\n", 29);
        return NULL;
    }

    char* old_brk = brk;
    brk += size;
    return old_brk;
}
