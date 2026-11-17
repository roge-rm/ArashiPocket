/* Arashi Script interpreter, from Parser.c by Juri Munkki (GameCompiler
   (jam)). The script (TEXT 2128 + 2129) is compiled one statement at a time
   as levels are reached; each variable keeps a small stack program that is
   run lazily, at most once per level, when the game asks for its value. */
#ifndef CORE_PARSER_H
#define CORE_PARSER_H

#include "types.h"

enum {
	kLexPlus, kLexMinus, kLexAbs,
	kLexMultiply, kLexDivide, kLexModulo, kLexPower,
	kLexLt, kLexGt, kLexEq,
	kLexMin, kLexMax,
	kLexOpenParen, kLexCloseParen,
	kLexRowOffset, kLexConstant,
	kUnaryMinus, kLexVariable,
	kLexFun0, kLexFun1,
	kLexLevel,
	kAssignment,
	kParseError, kLexEof
};

/* Tokens of the keywords, added to the dictionary first. Script variables
   follow from firstVariable in the order they first appear. */
enum {
	kMinKey, kMaxKey,
	kRandomKey, kSinKey, kCosKey,
	kIntKey, kRoundKey,
	kLevelKey, firstVariable
};

/* Reads the script through plat_resource and sets up the symbol tables.
   Returns 0 if the script or memory is missing. */
cbool parser_alloc(void);
void  parser_free(void);

void   parser_load_level(int16 level);
double parser_eval(int32 token);
void   parser_write(int32 token, double value);

/* Set once a parse error has stopped compiling. */
cbool parser_error(void);
/* The script name of a token, for dumps. */
const uint8 *parser_token_name(int32 token, int16 *len);

#endif
