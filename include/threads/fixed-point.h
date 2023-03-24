#include <stdint.h>
#define F (1<<14)

int int_fp(int n){
	return n*F;
}
int fp_int(int n){
	return n/F;
}
int fp_int_round(int x){
	if (x>=0){
		return (x+F/2)/F;
	} else {
		return (x-F/2)/F;
	}
}
int add_fp(int x, int y){
	return x+y;
}
int sub_fp(int x, int y){
	return x-y;
}
int mul_fp(int x, int y){
	return ((int64_t)x)*y/F;
}
int div_fp(int x, int y){
	return ((int64_t) x)*F/y;
}
int add_int(int x, int y){
	return x+y*F;
}
int sub_int(int x, int y){
	return x-y*F;
}
int mul_int (int x, int y){
	return x*y;
}
int div_int(int x, int y){
	return x/y;
}