#include <stdio.h>
#include <iostream>
#include <time.h>
#include "sm2.h"
#include "sm3.h"
extern "C" {
#include "miracl.h"
#include "mirdef.h"
}

/******************************************************************************
Function: key_generate
Description: Generate the private key and public key
Calls:
Called By:
Input: big p, big a, big b, big n, big x, big y, ECC *ecc
Output: private_key[32], pk_x[32], pk_y[32], Za[256]
Return: null
Others:
*******************************************************************************/
void key_generate(big p, big a, big b, big n, big x, big y, ECC* ecc, unsigned char* private_key, unsigned char* pk_x, unsigned char *pk_y, unsigned char *Za){
	unsigned char Z[200] = { 0x00, 0x03, 's', 'm', '2' };	//ENTL||ID
	big k, kg_x, kg_y;
	epoint *g;
	miracl *mip = mirsys(300, 0);	//因局部变量需要初始化
	k = mirvar(0);
	kg_x = mirvar(0);
	kg_y = mirvar(0);
	mip->IOBASE = 16;
	ecurve_init(a, b, p, MR_PROJECTIVE);	//初始化椭圆曲线
	g = epoint_init();	//初始化点g
	epoint_set(x, y, 0, g);
	irand((unsigned int)time(NULL));
	bigrand(n, k);	//生成随机数k,作为私钥
	ecurve_mult(k, g, g);	//计算[k]G
	epoint_get(g, kg_x, kg_y);
	big_to_bytes(32, k, (char*)private_key, TRUE);
	big_to_bytes(32, kg_x, (char*)pk_x, TRUE);
	big_to_bytes(32, kg_y, (char*)pk_y, TRUE);

	//Za = H256(ENTLA ∥ IDA ∥ a ∥ b ∥ xG ∥yG ∥ xA ∥ yA)。
	memcpy(Z, ecc->a, 32);
	memcpy(Z + 32, ecc->b, 32);
	memcpy(Z + 64, ecc->x, 32);
	memcpy(Z + 96, ecc->y, 32);
	memcpy(Z + 128, pk_x, 32);
	memcpy(Z + 160, pk_y, 32);
	SM3_256(Z, 195, Za);

	mirkill(k);
	mirkill(kg_x);
	mirkill(kg_y);
	epoint_free(g);
	mirexit();

}

/******************************************************************************
Function: sm2_sign
Description: Sign the message
Calls:
Called By:
Input: big p, big a, big b, big n, big x, big y, ECC *ecc, msg, msg_len, Za, private_key[32]
Output: sign_r[32], sign_s[32]
Return: null
Others:
*******************************************************************************/
void sm2_sign(big p, big a, big b, big n, big x, big y, ECC* ecc, unsigned char *msg, int msg_len, unsigned char *Za, unsigned char* private_key, unsigned char *sign_r, unsigned char *sign_s) {
	unsigned char m_hash[32] = { 0 };	//待签名消息的hash值
	unsigned char *m = (unsigned char*)malloc((msg_len + 32) * sizeof(unsigned char));
	memcpy(m , Za, 32);
	memcpy(m + 32, msg, msg_len);
	SM3_256(m, msg_len + 32, m_hash);	//计算hash值
	big k, r, s, e, key, kg_x, kg_y, temp, temp2;
	epoint *g;
	miracl *mip = mirsys(300, 0);
	k = mirvar(0);
	r = mirvar(0);
	s = mirvar(0);
	e = mirvar(0);
	key = mirvar(0);
	kg_x = mirvar(0);
	kg_y = mirvar(0);
	temp = mirvar(0);
	temp2 = mirvar(0);
	mip->IOBASE = 16;
	bytes_to_big(32, (char*)private_key, key);
	bytes_to_big(32, (char*)m_hash, e);
	ecurve_init(a, b, p, MR_PROJECTIVE);	//初始化椭圆曲线
	g = epoint_init();	//初始化点g
	epoint_set(x, y, 0, g);

	while (1) {
		irand((unsigned int)time(NULL));
		bigrand(n, k);	//生成随机数k
		ecurve_mult(k, g, g);	//计算[k]G
		epoint_get(g, kg_x, kg_y);	//获取[k]G的坐标值
		add(e, kg_x, r);
		divide(r, n, n);	//r = (e + x) mod n
		add(r, k, temp);
		if (r->len == 0 || mr_compare(temp, n) == 0)
			continue;
		incr(key, 1, temp);	//temp = key+1
		xgcd(temp, n, temp, temp, temp); 
		multiply(r, key, temp2);	//temp2 = r*key 
		divide(temp2, n, n);
		if (mr_compare(k, temp2) >= 0)
		{
			subtract(k, temp2, temp2);
		}
		else						//Attention!
		{
			subtract(n, temp2, temp2);
			add(k, temp2, temp2);
		}
		mad(temp, temp2, n, n, n, s);	//s = (temp * (k - r*key)) mod n
		if (s->len == 0)
			continue;
		big_to_bytes(32, r, (char*)sign_r, TRUE);
		big_to_bytes(32, s, (char*)sign_s, TRUE);
		break;
	}

	mirkill(e);
	mirkill(r);
	mirkill(s);
	mirkill(k);
	mirkill(kg_x);
	mirkill(kg_y);
	mirkill(key);
	mirkill(temp);
	mirkill(temp2);
	epoint_free(g);
	mirexit();
}

/******************************************************************************
Function: sm2_verify
Description: Verify the signature
Calls:
Called By:
Input: big p, big a, big b, big n, big x, big y, ECC *ecc, msg, Za, sign_r[32], sign_s[32], pk_x[32], pk_y[32]
Output: null
Return: 1 if  pass, 0 else
Others:
*******************************************************************************/
int sm2_verify(big p, big a, big b, big n, big x, big y, ECC* ecc, unsigned char *msg, int msg_len, unsigned char *Za, unsigned char  *sign_r, unsigned char *sign_s, unsigned char *pk_x,unsigned char *pk_y){
	unsigned char m_hash[32] = { 0 };
	unsigned char *m = (unsigned char*)malloc((msg_len + 32) * sizeof(unsigned char));
	memcpy(m, Za, 32);
	memcpy(m + 32, msg, msg_len);
	SM3_256(m, msg_len + 32, m_hash);
	big k, r, s, e, pkx, pky, temp;
	epoint *g, *pa;
	miracl *mip = mirsys(300, 0);
	k = mirvar(0);
	r = mirvar(0);
	s = mirvar(0);
	e = mirvar(0);
	pkx = mirvar(0);
	pky = mirvar(0);
	temp = mirvar(0);
	mip->IOBASE = 16;
	bytes_to_big(32, (char*)m_hash, e);
	bytes_to_big(32, (char*)sign_r, r);
	bytes_to_big(32, (char*)sign_s, s);
	bytes_to_big(32, (char*)pk_x, pkx);
	bytes_to_big(32, (char*)pk_y, pky);
	ecurve_init(a, b, p, MR_PROJECTIVE);
	g = epoint_init();
	pa = epoint_init();
	epoint_set(x, y, 0, g);
	epoint_set(pkx, pky, 0, pa);
	if (r->len == 0 || mr_compare(r, n) >= 0)
		return 0;
	if (s->len == 0 || mr_compare(s, n) >= 0)
		return 0;
	add(r, s, temp);
	divide(temp, n, n);		//t = (r + s) mod n
	if (temp->len == 0)
		return 0;
	ecurve_mult2(s, g, temp, pa, g);	//[s]G + [t]Pa
	epoint_get(g, x, y);
	add(e, x, x);
	divide(x, n, n);	// (e + x) mod n
	if (mr_compare(x, r) != 0)
		return 0;
	return 1;
}