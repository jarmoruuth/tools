/**********************************************************************\
 *
 *		ECTYPE.C
 *
 *	New definition for C's _ctype table, called _ectype. 
 *	Table supports IBM PC extended ascii character set
 *	or 7 bit scandinavian ASCII character set.
 *
 *	A new table _diffcase is used to change case of a letter.
 *	With character as an index to the table, you can get it's
 *	opposite case version, if there is one, or otherwise just
 *	the same character.
 *
 *	Author : J.Ruuth 19-Jan-1988
 *
 *	Copyright (C) 1988 by J.Ruuth
\**********************************************************************/

#include "ectype.h"

/* Select which character set is used */

#if defined(DOS) || defined(OS2) || defined(NT)
#define	ECTYPE_IBMPC	/* Use IBM PC extended ASCII character set */
#else
#define	ECTYPE_SCAND	/* Use 7 bit scandinavian ASCII character set */
#endif

#ifdef	ECTYPE_IBMPC

#define	IBM_UPPER	_UPPER
#define	IBM_LOWER	_LOWER
#define	IBM_DIFF_129	154
#define	IBM_DIFF_130	144
#define	IBM_DIFF_132	142
#define IBM_DIFF_134	143
#define	IBM_DIFF_142	132
#define	IBM_DIFF_143	134
#define	IBM_DIFF_144	130
#define	IBM_DIFF_145	146
#define	IBM_DIFF_146	145
#define	IBM_DIFF_148	153
#define	IBM_DIFF_153	148
#define	IBM_DIFF_154	129
#define	IBM_DIFF_164	165
#define	IBM_DIFF_165	164

#else

#define	IBM_UPPER	0
#define	IBM_LOWER	0
#define	IBM_DIFF_129	129
#define	IBM_DIFF_130	130
#define	IBM_DIFF_132	132
#define IBM_DIFF_134	134
#define	IBM_DIFF_142	142
#define	IBM_DIFF_143	143
#define	IBM_DIFF_144	144
#define	IBM_DIFF_145	145
#define	IBM_DIFF_146	146
#define	IBM_DIFF_148	148
#define	IBM_DIFF_153	153
#define	IBM_DIFF_154	154
#define	IBM_DIFF_164	164
#define	IBM_DIFF_165	165

#endif

#ifdef	ECTYPE_SCAND

#define	SCAND_UPPER	_UPPER
#define	SCAND_LOWER	_LOWER
#define SCAND_DIFF_91	123
#define SCAND_DIFF_92	124
#define SCAND_DIFF_93	125
#define SCAND_DIFF_94	126
#define SCAND_DIFF_123	91
#define SCAND_DIFF_124	92
#define SCAND_DIFF_125	93
#define SCAND_DIFF_126	94

#else

#define	SCAND_UPPER	_PUNCT
#define	SCAND_LOWER	_PUNCT
#define SCAND_DIFF_91	91
#define SCAND_DIFF_92	92
#define SCAND_DIFF_93	93
#define SCAND_DIFF_94	94
#define SCAND_DIFF_123	123
#define SCAND_DIFF_124	124
#define SCAND_DIFF_125	125
#define SCAND_DIFF_126	126

#endif

unsigned char _ectype[] = {
	/*  0*/ _CONTROL,
	/*  1*/ _CONTROL,
	/*  2*/ _CONTROL,
	/*  3*/ _CONTROL,
	/*  4*/ _CONTROL,
	/*  5*/ _CONTROL,
	/*  6*/ _CONTROL,
	/*  7*/ _CONTROL,
	/*  8*/ _CONTROL,
	/*  9*/ _SPACE|_CONTROL,
	/* 10*/ _SPACE|_CONTROL,
	/* 11*/ _SPACE|_CONTROL,
	/* 12*/ _SPACE|_CONTROL,
	/* 13*/ _SPACE|_CONTROL,
	/* 14*/ _CONTROL,
	/* 15*/ _CONTROL,
	/* 16*/ _CONTROL,
	/* 17*/ _CONTROL,
	/* 18*/ _CONTROL,
	/* 19*/ _CONTROL,
	/* 20*/ _CONTROL,
	/* 21*/ _CONTROL,
	/* 22*/ _CONTROL,
	/* 23*/ _CONTROL,
	/* 24*/ _CONTROL,
	/* 25*/ _CONTROL,
	/* 26*/ _CONTROL,
	/* 27*/ _CONTROL,
	/* 28*/ _CONTROL,
	/* 29*/ _CONTROL,
	/* 30*/ _CONTROL,
	/* 31*/ _CONTROL,
	/* 32*/ _SPACE|_BLANK,
	/* 33*/ _PUNCT,
	/* 34*/ _PUNCT,
	/* 35*/ _PUNCT,
	/* 36*/ _PUNCT,
	/* 37*/ _PUNCT,
	/* 38*/ _PUNCT,
	/* 39*/ _PUNCT,
	/* 40*/ _PUNCT,
	/* 41*/ _PUNCT,
	/* 42*/ _PUNCT,
	/* 43*/ _PUNCT,
	/* 44*/ _PUNCT,
	/* 45*/ _PUNCT,
	/* 46*/ _PUNCT,
	/* 47*/ _PUNCT,
	/* 48*/ _DIGIT|_HEX,
	/* 49*/ _DIGIT|_HEX,
	/* 50*/ _DIGIT|_HEX,
	/* 51*/ _DIGIT|_HEX,
	/* 52*/ _DIGIT|_HEX,
	/* 53*/ _DIGIT|_HEX,
	/* 54*/ _DIGIT|_HEX,
	/* 55*/ _DIGIT|_HEX,
	/* 56*/ _DIGIT|_HEX,
	/* 57*/ _DIGIT|_HEX,
	/* 58*/ _PUNCT,
	/* 59*/ _PUNCT,
	/* 60*/ _PUNCT,
	/* 61*/ _PUNCT,
	/* 62*/ _PUNCT,
	/* 63*/ _PUNCT,
	/* 64*/ _PUNCT,
	/* 65*/ _UPPER|_HEX,
	/* 66*/ _UPPER|_HEX,
	/* 67*/ _UPPER|_HEX,
	/* 68*/ _UPPER|_HEX,
	/* 69*/ _UPPER|_HEX,
	/* 70*/ _UPPER|_HEX,
	/* 71*/ _UPPER,
	/* 72*/ _UPPER,
	/* 73*/ _UPPER,
	/* 74*/ _UPPER,
	/* 75*/ _UPPER,
	/* 76*/ _UPPER,
	/* 77*/ _UPPER,
	/* 78*/ _UPPER,
	/* 79*/ _UPPER,
	/* 80*/ _UPPER,
	/* 81*/ _UPPER,
	/* 82*/ _UPPER,
	/* 83*/ _UPPER,
	/* 84*/ _UPPER,
	/* 85*/ _UPPER,
	/* 86*/ _UPPER,
	/* 87*/ _UPPER,
	/* 88*/ _UPPER,
	/* 89*/ _UPPER,
	/* 90*/ _UPPER,
	/* 91*/ SCAND_UPPER,
	/* 92*/ SCAND_UPPER,
	/* 93*/ SCAND_UPPER,
	/* 94*/ SCAND_UPPER,
	/* 95*/ _PUNCT,
	/* 96*/ _PUNCT,
	/* 97*/ _LOWER|_HEX,
	/* 98*/ _LOWER|_HEX,
	/* 99*/ _LOWER|_HEX,
	/*100*/ _LOWER|_HEX,
	/*101*/ _LOWER|_HEX,
	/*102*/ _LOWER|_HEX,
	/*103*/ _LOWER,
	/*104*/ _LOWER,
	/*105*/ _LOWER,
	/*106*/ _LOWER,
	/*107*/ _LOWER,
	/*108*/ _LOWER,
	/*109*/ _LOWER,
	/*110*/ _LOWER,
	/*111*/ _LOWER,
	/*112*/ _LOWER,
	/*113*/ _LOWER,
	/*114*/ _LOWER,
	/*115*/ _LOWER,
	/*116*/ _LOWER,
	/*117*/ _LOWER,
	/*118*/ _LOWER,
	/*119*/ _LOWER,
	/*120*/ _LOWER,
	/*121*/ _LOWER,
	/*122*/ _LOWER,
	/*123*/ SCAND_LOWER,
	/*124*/ SCAND_LOWER,
	/*125*/ SCAND_LOWER,
	/*126*/ SCAND_LOWER,
	/*127*/ _CONTROL,
	/*128*/ 0,
	/*129*/ IBM_LOWER,
	/*130*/ IBM_LOWER,
	/*131*/ 0,
	/*132*/ IBM_LOWER,
	/*133*/ 0,
	/*134*/ IBM_LOWER,
	/*135*/ 0,
	/*136*/ 0,
	/*137*/ 0,
	/*138*/ 0,
	/*139*/ 0,
	/*140*/ 0,
	/*141*/ 0,
	/*142*/ IBM_UPPER,
	/*143*/ IBM_UPPER,
	/*144*/ IBM_UPPER,
	/*145*/ IBM_LOWER,
	/*146*/ IBM_UPPER,
	/*147*/ 0,
	/*148*/ IBM_LOWER,
	/*149*/ 0,
	/*150*/ 0,
	/*151*/ 0,
	/*152*/ 0,
	/*153*/ IBM_UPPER,
	/*154*/ IBM_UPPER,
	/*155*/ 0,
	/*156*/ 0,
	/*157*/ 0,
	/*158*/ 0,
	/*159*/ 0,
	/*160*/ 0,
	/*161*/ 0,
	/*162*/ 0,
	/*163*/ 0,
	/*164*/ IBM_LOWER,
	/*165*/ IBM_UPPER,
	/*166*/ 0,
	/*167*/ 0,
	/*168*/ 0,
	/*169*/ 0,
	/*170*/ 0,
	/*171*/ 0,
	/*172*/ 0,
	/*173*/ 0,
	/*174*/ 0,
	/*175*/ 0,
	/*176*/ 0,
	/*177*/ 0,
	/*178*/ 0,
	/*179*/ 0,
	/*180*/ 0,
	/*181*/ 0,
	/*182*/ 0,
	/*183*/ 0,
	/*184*/ 0,
	/*185*/ 0,
	/*186*/ 0,
	/*187*/ 0,
	/*188*/ 0,
	/*189*/ 0,
	/*190*/ 0,
	/*191*/ 0,
	/*192*/ 0,
	/*193*/ 0,
	/*194*/ 0,
	/*195*/ 0,
	/*196*/ 0,
	/*197*/ 0,
	/*198*/ 0,
	/*199*/ 0,
	/*200*/ 0,
	/*201*/ 0,
	/*202*/ 0,
	/*203*/ 0,
	/*204*/ 0,
	/*205*/ 0,
	/*206*/ 0,
	/*207*/ 0,
	/*208*/ 0,
	/*209*/ 0,
	/*210*/ 0,
	/*211*/ 0,
	/*212*/ 0,
	/*213*/ 0,
	/*214*/ 0,
	/*215*/ 0,
	/*216*/ 0,
	/*217*/ 0,
	/*218*/ 0,
	/*219*/ 0,
	/*220*/ 0,
	/*221*/ 0,
	/*222*/ 0,
	/*223*/ 0,
	/*224*/ 0,
	/*225*/ 0,
	/*226*/ 0,
	/*227*/ 0,
	/*228*/ 0,
	/*229*/ 0,
	/*230*/ 0,
	/*231*/ 0,
	/*232*/ 0,
	/*233*/ 0,
	/*234*/ 0,
	/*235*/ 0,
	/*236*/ 0,
	/*237*/ 0,
	/*238*/ 0,
	/*239*/ 0,
	/*240*/ 0,
	/*241*/ 0,
	/*242*/ 0,
	/*243*/ 0,
	/*244*/ 0,
	/*245*/ 0,
	/*246*/ 0,
	/*247*/ 0,
	/*248*/ 0,
	/*249*/ 0,
	/*250*/ 0,
	/*251*/ 0,
	/*252*/ 0,
	/*253*/ 0,
	/*254*/ 0,
	/*255*/ 0
};

unsigned char _diffcase[] = {
	/*  0*/   0,
	/*  1*/   1,
	/*  2*/   2,
	/*  3*/   3,
	/*  4*/   4,
	/*  5*/   5,
	/*  6*/   6,
	/*  7*/   7,
	/*  8*/   8,
	/*  9*/   9,
	/* 10*/  10,
	/* 11*/  11,
	/* 12*/  12,
	/* 13*/  13,
	/* 14*/  14,
	/* 15*/  15,
	/* 16*/  16,
	/* 17*/  17,
	/* 18*/  18,
	/* 19*/  19,
	/* 20*/  20,
	/* 21*/  21,
	/* 22*/  22,
	/* 23*/  23,
	/* 24*/  24,
	/* 25*/  25,
	/* 26*/  26,
	/* 27*/  27,
	/* 28*/  28,
	/* 29*/  29,
	/* 30*/  30,
	/* 31*/  31,
	/* 32*/  32,
	/* 33*/  33,
	/* 34*/  34,
	/* 35*/  35,
	/* 36*/  36,
	/* 37*/  37,
	/* 38*/  38,
	/* 39*/  39,
	/* 40*/  40,
	/* 41*/  41,
	/* 42*/  42,
	/* 43*/  43,
	/* 44*/  44,
	/* 45*/  45,
	/* 46*/  46,
	/* 47*/  47,
	/* 48*/  48,
	/* 49*/  49,
	/* 50*/  50,
	/* 51*/  51,
	/* 52*/  52,
	/* 53*/  53,
	/* 54*/  54,
	/* 55*/  55,
	/* 56*/  56,
	/* 57*/  57,
	/* 58*/  58,
	/* 59*/  59,
	/* 60*/  60,
	/* 61*/  61,
	/* 62*/  62,
	/* 63*/  63,
	/* 64*/  64,
	/* 65*/  97,
	/* 66*/  98,
	/* 67*/  99,
	/* 68*/ 100,
	/* 69*/ 101,
	/* 70*/ 102,
	/* 71*/ 103,
	/* 72*/ 104,
	/* 73*/ 105,
	/* 74*/ 106,
	/* 75*/ 107,
	/* 76*/ 108,
	/* 77*/ 109,
	/* 78*/ 110,
	/* 79*/ 111,
	/* 80*/ 112,
	/* 81*/ 113,
	/* 82*/ 114,
	/* 83*/ 115,
	/* 84*/ 116,
	/* 85*/ 117,
	/* 86*/ 118,
	/* 87*/ 119,
	/* 88*/ 120,
	/* 89*/ 121,
	/* 90*/ 122,
	/* 91*/  SCAND_DIFF_91,
	/* 92*/  SCAND_DIFF_92,
	/* 93*/  SCAND_DIFF_93,
	/* 94*/  SCAND_DIFF_94,
	/* 95*/  95,
	/* 96*/  96,
	/* 97*/  65,
	/* 98*/  66,
	/* 99*/  67,
	/*100*/  68,
	/*101*/  69,
	/*102*/  70,
	/*103*/  71,
	/*104*/  72,
	/*105*/  73,
	/*106*/  74,
	/*107*/  75,
	/*108*/  76,
	/*109*/  77,
	/*110*/  78,
	/*111*/  79,
	/*112*/  80,
	/*113*/  81,
	/*114*/  82,
	/*115*/  83,
	/*116*/  84,
	/*117*/  85,
	/*118*/  86,
	/*119*/  87,
	/*120*/  88,
	/*121*/  89,
	/*122*/  90,
	/*123*/ SCAND_DIFF_123,
	/*124*/ SCAND_DIFF_124,
	/*125*/ SCAND_DIFF_125,
	/*126*/ SCAND_DIFF_126,
	/*127*/ 127,
	/*128*/ 128,
	/*129*/ IBM_DIFF_129,
	/*130*/ IBM_DIFF_130,
	/*131*/ 131,
	/*132*/ IBM_DIFF_132,
	/*133*/ 133,
	/*134*/ IBM_DIFF_134,
	/*135*/ 135,
	/*136*/ 136,
	/*137*/ 137,
	/*138*/ 138,
	/*139*/ 139,
	/*140*/ 140,
	/*141*/ 141,
	/*142*/ IBM_DIFF_142,
	/*143*/ IBM_DIFF_143,
	/*144*/ IBM_DIFF_144,
	/*145*/ IBM_DIFF_145,
	/*146*/ IBM_DIFF_146,
	/*147*/ 147,
	/*148*/ IBM_DIFF_148,
	/*149*/ 149,
	/*150*/ 150,
	/*151*/ 151,
	/*152*/ 152,
	/*153*/ IBM_DIFF_153,
	/*154*/ IBM_DIFF_154,
	/*155*/ 155,
	/*156*/ 156,
	/*157*/ 157,
	/*158*/ 158,
	/*159*/ 159,
	/*160*/ 160,
	/*161*/ 161,
	/*162*/ 162,
	/*163*/ 163,
	/*164*/ IBM_DIFF_164,
	/*165*/ IBM_DIFF_165,
	/*166*/ 166,
	/*167*/ 167,
	/*168*/ 168,
	/*169*/ 169,
	/*170*/ 170,
	/*171*/ 171,
	/*172*/ 172,
	/*173*/ 173,
	/*174*/ 174,
	/*175*/ 175,
	/*176*/ 176,
	/*177*/ 177,
	/*178*/ 178,
	/*179*/ 179,
	/*180*/ 180,
	/*181*/ 181,
	/*182*/ 182,
	/*183*/ 183,
	/*184*/ 184,
	/*185*/ 185,
	/*186*/ 186,
	/*187*/ 187,
	/*188*/ 188,
	/*189*/ 189,
	/*190*/ 190,
	/*191*/ 191,
	/*192*/ 192,
	/*193*/ 193,
	/*194*/ 194,
	/*195*/ 195,
	/*196*/ 196,
	/*197*/ 197,
	/*198*/ 198,
	/*199*/ 199,
	/*200*/ 200,
	/*201*/ 201,
	/*202*/ 202,
	/*203*/ 203,
	/*204*/ 204,
	/*205*/ 205,
	/*206*/ 206,
	/*207*/ 207,
	/*208*/ 208,
	/*209*/ 209,
	/*210*/ 210,
	/*211*/ 211,
	/*212*/ 212,
	/*213*/ 213,
	/*214*/ 214,
	/*215*/ 215,
	/*216*/ 216,
	/*217*/ 217,
	/*218*/ 218,
	/*219*/ 219,
	/*220*/ 220,
	/*221*/ 221,
	/*222*/ 222,
	/*223*/ 223,
	/*224*/ 224,
	/*225*/ 225,
	/*226*/ 226,
	/*227*/ 227,
	/*228*/ 228,
	/*229*/ 229,
	/*230*/ 230,
	/*231*/ 231,
	/*232*/ 232,
	/*233*/ 233,
	/*234*/ 234,
	/*235*/ 235,
	/*236*/ 236,
	/*237*/ 237,
	/*238*/ 238,
	/*239*/ 239,
	/*240*/ 240,
	/*241*/ 241,
	/*242*/ 242,
	/*243*/ 243,
	/*244*/ 244,
	/*245*/ 245,
	/*246*/ 246,
	/*247*/ 247,
	/*248*/ 248,
	/*249*/ 249,
	/*250*/ 250,
	/*251*/ 251,
	/*252*/ 252,
	/*253*/ 253,
	/*254*/ 254,
	/*255*/ 255
};
