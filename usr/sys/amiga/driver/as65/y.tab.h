
typedef union
#ifdef __cplusplus
	YYSTYPE
#endif
 {
	struct expr	*expr;
	uint		num;
	char		*str;
} YYSTYPE;
extern YYSTYPE yylval;
# define CONSTANT 257
# define SYMBOL 258
# define STRING 259
# define SEGMENT 260
# define BLOCK 261
# define BYTE 262
# define WORD 263
# define LSHIFT 264
# define RSHIFT 265
# define A 266
# define X 267
# define Y 268
# define ADC 269
# define AND 270
# define ASL 271
# define BCC 272
# define BCS 273
# define BEQ 274
# define BIT 275
# define BMI 276
# define BNE 277
# define BPL 278
# define BRK 279
# define BVC 280
# define BVS 281
# define CLC 282
# define CLD 283
# define CLI 284
# define CLV 285
# define CMP 286
# define CPX 287
# define CPY 288
# define DEC 289
# define DEX 290
# define DEY 291
# define EOR 292
# define INC 293
# define INX 294
# define INY 295
# define JMP 296
# define JSR 297
# define LDA 298
# define LDX 299
# define LDY 300
# define LSR 301
# define NOP 302
# define ORA 303
# define PHA 304
# define PHP 305
# define PLA 306
# define PLP 307
# define ROL 308
# define ROR 309
# define RTI 310
# define RTS 311
# define SBC 312
# define SEC 313
# define SED 314
# define SEI 315
# define STA 316
# define STX 317
# define STY 318
# define STZ 319
# define TAX 320
# define TAY 321
# define TSX 322
# define TXA 323
# define TXS 324
# define TYA 325
