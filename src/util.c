
#include "util.h"

int encrypt(char* msg, int key){
	char* chr_p = msg;
	while(*chr_p != '\0'){
		int chr = *chr_p;
		if(32 < chr && chr < 127){
			chr = (chr - 33 + key + (127-33))%(127-33) + 33;//the extra plus ensures that we do not get c weird modulo of negative nubmers
			*chr_p = (char)chr;
		}
		chr_p++;
	}
	return 1;
}
int decrypt(char* msg, int key){
	return encrypt(msg, -key);
}
