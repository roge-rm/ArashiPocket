/* Arashi Script interpreter, ported from Parser.c (Arashi 1.1.1, Juri
   Munkki). Same lexer, grammar, token numbering and evaluation order as the
   original; the Think C objects are now strdict and tagbase, Handles are
   plat_alloc blocks, and the maths is cmath.

   Grammar, loosest first: compare (< > =), min/max, + -, * / %, ^, factor.
   "a min b" is a lower bound (the larger of the two) and "a max b" an
   upper bound, as in the original. */
#include "parser.h"
#include "internalvars.h"
#include "strdict.h"
#include "tagbase.h"
#include "cmath.h"
#include "qdrandom.h"
#include "plat.h"

typedef struct {
	int16 kind;
	union {
		double floating;        /* constants */
		int32  offset;          /* row offsets (unused by the lexer) */
		int32  token;           /* variables and functions */
		int32  level;           /* level statements */
	} value;
} LexSymbol;

typedef struct {
	double value;
	int16  calc_level;          /* level the value was worked out for */
} VariableValue;

#define STACK_SIZE 256

static struct {
	const uint8 *input;
	LexSymbol   *out;           /* program of the statement being compiled */
	int16        out_used, out_cap;
	LexSymbol    lookahead;
	int16        stack_depth, stack_max;
} pv;

static const uint8 *script;    /* ASCR 128, held from parser_alloc to parser_free */
static double  *stack, *stack_p;
static StrDict  sym_table;
static TagBase  variable_base, program_base;
static int32    last_keyword, last_variable;
static int16    current_level = 0;
static int16    from_level = 32000;
static int16    up_to_level = -1;
static cbool    had_error;
static cbool    ready;

SEG_SCRIPT static void parse_compare(void);

SEG_SCRIPT static void init_symbols(void)
{
	/* In .data on purpose: a table of string pointers would point into this segment. */
	static char keys[] = "min\0max\0random\0sin\0cos\0int\0round\0level";
	const char *key = keys;
	int16 i, n;

	strdict_init(&sym_table);
	for (i = 0; i < 8; i++) {
		for (n = 0; key[n]; n++)
			;
		last_keyword = strdict_add(&sym_table, (const uint8 *)key, n);
		key += n + 1;
	}
	last_variable = last_keyword;
	tagbase_init(&variable_base);
	tagbase_init(&program_base);
}

SEG_SCRIPT static void reset_variables(void)
{
	strdict_free(&sym_table);
	tagbase_free(&variable_base);
	tagbase_free(&program_base);
	init_symbols();
}

SEG_SCRIPT static void create_variable(int32 token)
{
	VariableValue zero;

	zero.value = 0;
	zero.calc_level = -1;
	if (!tagbase_write(&variable_base, token, sizeof zero, &zero))
		had_error = 1;
}

SEG_SCRIPT static void variable_to_keyword(LexSymbol *sym)
{
	switch (sym->value.token) {
	case kMinKey:
		sym->kind = kLexMin;
		break;
	case kMaxKey:
		sym->kind = kLexMax;
		break;
	case kRandomKey:
		sym->kind = kLexFun0;
		break;
	case kSinKey:
	case kCosKey:
	case kIntKey:
	case kRoundKey:
		sym->kind = kLexFun1;
		break;
	case kLevelKey:
		sym->kind = kLexLevel;
		break;
	}
}

/* A letter, then letters, digits and _ . [ \ ] ^ ` { | }. */
SEG_SCRIPT static int16 match_variable(const uint8 *s)
{
	uint8 c = s[0];
	int16 n = 0;

	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
		n++;
		for (;;) {
			c = s[n];
			if ((c >= 'a' && c <= '}') || c == '_' ||
			    (c >= 'A' && c <= ']') || c == '.' ||
			    (c >= '0' && c <= '9'))
				n++;
			else
				break;
		}
	}
	return n;
}

/* Exponent digits after the e of a number: [+-]digits. */
SEG_SCRIPT static int16 match_exponent(const uint8 *s)
{
	int16 n = 0;
	uint8 c = s[0];

	if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
		n++;
		while (s[n] >= '0' && s[n] <= '9')
			n++;
	}
	return n;
}

/* [+-]digits[.digits][(e|E) exponent]; an e without exponent digits is
   left out of the number. */
SEG_SCRIPT static int16 match_float(const uint8 *s)
{
	enum { initial, integer_part, decimal_part, exponent, done };
	int16 n = 0, state = initial;
	uint8 c;

	while (state != done) {
		c = s[n];
		if (c == 0)
			state = done;
		switch (state) {
		case initial:
			if (c == '+' || c == '-' || (c >= '0' && c <= '9')) {
				n++;
				state = integer_part;
			} else
				state = done;
			break;
		case integer_part:
			if (c >= '0' && c <= '9')
				n++;
			else if (c == 'e' || c == 'E')
				state = exponent;
			else if (c == '.') {
				n++;
				state = decimal_part;
			} else
				state = done;
			break;
		case decimal_part:
			if (c >= '0' && c <= '9')
				n++;
			else if (c == 'e' || c == 'E')
				state = exponent;
			else
				state = done;
			break;
		case exponent: {
			int16 len = match_exponent(s + n + 1);
			if (len > 0)
				n = (int16)(n + len + 1);
			state = done;
			break;
		}
		}
	}
	return n;
}

SEG_SCRIPT static void emit(const LexSymbol *op, int16 stack_change)
{
	pv.stack_depth = (int16)(pv.stack_depth + stack_change);
	if (pv.stack_depth > pv.stack_max)
		pv.stack_max = pv.stack_depth;

	if (pv.out_used == pv.out_cap) {
		int16 cap = (int16)(pv.out_cap + 32), i;
		LexSymbol *n = (LexSymbol *)plat_alloc((uint32)cap * sizeof(LexSymbol));
		if (!n) {
			had_error = 1;
			return;
		}
		for (i = 0; i < pv.out_used; i++)
			n[i] = pv.out[i];
		if (pv.out)
			plat_free(pv.out);
		pv.out = n;
		pv.out_cap = cap;
	}
	pv.out[pv.out_used++] = *op;
}

SEG_SCRIPT static void skip_comment(void)
{
	pv.input += 2;
	if (pv.input[0] != 0) {
		while (pv.input[0] != 0 && !(pv.input[0] == '*' && pv.input[1] == '/'))
			pv.input++;
		if (pv.input[0] != 0)
			pv.input += 2;
	}
}

/* Runs to the end of the line. The script has Mac line ends (CR, 13). */
SEG_SCRIPT static void skip_one_line_comment(void)
{
	pv.input += 2;
	if (pv.input[0] != 0) {
		while (pv.input[0] != 0 && pv.input[0] != 13)
			pv.input++;
		if (pv.input[0] != 0)
			pv.input += 1;
	}
}

SEG_SCRIPT static void lex_read(LexSymbol *sym)
{
	uint8 c;
	int16 n;

	/* Skip spaces, returns, tabs and other control characters. */
	c = *pv.input;
	while (c <= 32 && c > 0)
		c = *(++pv.input);

	switch (c) {
	case '+':
		sym->kind = kLexPlus;
		pv.input++;
		break;
	case '-':
		sym->kind = kLexMinus;
		pv.input++;
		break;
	case '|':
		sym->kind = kLexAbs;
		pv.input++;
		break;
	case '*':
		sym->kind = kLexMultiply;
		pv.input++;
		break;
	case '/':
		if (pv.input[1] == '*') {
			skip_comment();
			lex_read(sym);
		} else if (pv.input[1] == '/') {
			skip_one_line_comment();
			lex_read(sym);
		} else {
			sym->kind = kLexDivide;
			pv.input++;
		}
		break;
	case '%':
		sym->kind = kLexModulo;
		pv.input++;
		break;
	case '^':
		sym->kind = kLexPower;
		pv.input++;
		break;
	case '>':
		sym->kind = kLexGt;
		pv.input++;
		break;
	case '<':
		sym->kind = kLexLt;
		pv.input++;
		break;
	case '=':
		sym->kind = kLexEq;
		pv.input++;
		break;
	case '(':
		sym->kind = kLexOpenParen;
		pv.input++;
		break;
	case ')':
		sym->kind = kLexCloseParen;
		pv.input++;
		break;
	case 0:
		sym->kind = kLexEof;
		break;
	default:
		n = match_variable(pv.input);
		if (n > 0) {
			int16 token = strdict_find(&sym_table, pv.input, n);
			if (token < 0) {
				had_error = 1;
				sym->kind = kParseError;
				break;
			}
			sym->value.token = token;
			if (token > last_variable) {
				last_variable = token;
				create_variable(last_variable);
			}
			sym->kind = kLexVariable;
			if (token <= last_keyword)
				variable_to_keyword(sym);
			pv.input += n;
		} else {
			n = match_float(pv.input);
			if (n > 0) {
				sym->kind = kLexConstant;
				sym->value.floating = cm_strtod(pv.input, n);
				pv.input += n;
			}
		}
		if (n == 0)
			sym->kind = kParseError;
		break;
	}
}

SEG_SCRIPT static void lex_match(int16 kind)
{
	if (pv.lookahead.kind == kind)
		lex_read(&pv.lookahead);
	else
		pv.lookahead.kind = kParseError;
}

SEG_SCRIPT static void parse_factor(void)
{
	LexSymbol saved;

	saved.kind = 0;
	saved.value.floating = 0;
	switch (pv.lookahead.kind) {
	case kLexFun0:
	case kLexConstant:
	case kLexVariable:
	case kLexRowOffset:
		emit(&pv.lookahead, 1);
		lex_match(pv.lookahead.kind);
		break;
	case kLexFun1:
		saved = pv.lookahead;
		lex_match(pv.lookahead.kind);
		parse_factor();
		emit(&saved, 0);
		break;
	case kLexMinus:
		lex_match(pv.lookahead.kind);
		parse_factor();
		saved.kind = kUnaryMinus;
		emit(&saved, 0);
		break;
	case kLexAbs:
		lex_match(pv.lookahead.kind);
		parse_factor();
		saved.kind = kLexAbs;
		emit(&saved, 0);
		break;
	case kLexPlus:          /* no-op */
		lex_match(pv.lookahead.kind);
		parse_factor();
		break;
	case kLexOpenParen:
		lex_match(pv.lookahead.kind);
		parse_compare();
		if (pv.lookahead.kind == kLexCloseParen)
			lex_match(pv.lookahead.kind);
		else
			pv.lookahead.kind = kParseError;
		break;
	default:
		pv.lookahead.kind = kParseError;
		break;
	}
}

/* One left-associative binary level: operands from next, operators kinds
   a, b or c (-1 for none). */
SEG_SCRIPT static void parse_binary(void (*next)(void), int16 a, int16 b, int16 c)
{
	LexSymbol sym;

	sym.value.floating = 0;
	next();
	while (pv.lookahead.kind == a || pv.lookahead.kind == b || pv.lookahead.kind == c) {
		sym.kind = pv.lookahead.kind;
		lex_match(pv.lookahead.kind);
		next();
		emit(&sym, -1);
	}
}

SEG_SCRIPT static void parse_power(void)
{
	parse_binary(parse_factor, kLexPower, -1, -1);
}

SEG_SCRIPT static void parse_term(void)
{
	parse_binary(parse_power, kLexMultiply, kLexDivide, kLexModulo);
}

SEG_SCRIPT static void parse_expr(void)
{
	parse_binary(parse_term, kLexPlus, kLexMinus, -1);
}

SEG_SCRIPT static void parse_min_max(void)
{
	parse_binary(parse_expr, kLexMin, kLexMax, -1);
}

SEG_SCRIPT static void parse_compare(void)
{
	parse_binary(parse_min_max, kLexGt, kLexEq, kLexLt);
}

SEG_SCRIPT static void parse_statement(LexSymbol *statement)
{
	if (pv.lookahead.kind == kLexVariable) {
		*statement = pv.lookahead;
		lex_match(pv.lookahead.kind);
		if (pv.lookahead.kind == kLexEq) {
			lex_match(pv.lookahead.kind);
			if (statement->value.token > last_variable) {
				last_variable = statement->value.token;
				create_variable(last_variable);
			}
			parse_compare();
			statement->kind = kAssignment;
			emit(statement, -1);
		} else
			pv.lookahead.kind = kParseError;
	} else if (pv.lookahead.kind == kLexLevel) {
		*statement = pv.lookahead;
		lex_match(pv.lookahead.kind);
		if (pv.lookahead.kind == kLexConstant) {
			statement->value.level = (int32)pv.lookahead.value.floating;
			lex_match(pv.lookahead.kind);
		} else
			pv.lookahead.kind = kParseError;
	} else
		pv.lookahead.kind = kParseError;
}

SEG_SCRIPT static void setup_compiler(const uint8 *input)
{
	pv.input = input;
	pv.out_used = 0;
	pv.stack_depth = 0;
	pv.stack_max = 0;
}

SEG_SCRIPT void parser_write(int32 token, double value)
{
	VariableValue *var = (VariableValue *)tagbase_get(&variable_base, token);

	if (var) {
		var->calc_level = current_level;
		var->value = value;
	}
}

SEG_SCRIPT double parser_eval(int32 token)
{
	VariableValue *var = (VariableValue *)tagbase_get(&variable_base, token);
	const LexSymbol *prog;
	double *sp;

	if (!var)
		return 0;
	if (var->calc_level == current_level)
		return var->value;

	/* Marked first, so a variable that depends on itself sees its old
	   value. */
	var->calc_level = current_level;
	prog = (const LexSymbol *)tagbase_get(&program_base, token);
	if (!prog)
		return var->value;

	for (; prog->kind != kAssignment; prog++) {
		if (stack_p >= stack + STACK_SIZE - 2) {
			had_error = 1;      /* runaway recursion; the original had no check */
			return var->value;
		}
		sp = stack_p;
		switch (prog->kind) {
		case kLexConstant:
			*(++stack_p) = prog->value.floating;
			break;
		case kLexVariable: {
			double v = parser_eval(prog->value.token);
			*(++stack_p) = v;
			break;
		}
		case kLexPlus:     sp[-1] += sp[0]; stack_p--; break;
		case kLexMinus:    sp[-1] -= sp[0]; stack_p--; break;
		case kLexMultiply: sp[-1] = cm_mul(sp[-1], sp[0]); stack_p--; break;
		case kLexDivide:   sp[-1] = cm_div(sp[-1], sp[0]); stack_p--; break;

		/* Comparisons through cmath so NaN gives false everywhere. */
		case kLexGt: sp[-1] = cm_lt(sp[0], sp[-1]); stack_p--; break;
		case kLexLt: sp[-1] = cm_lt(sp[-1], sp[0]); stack_p--; break;
		case kLexEq: sp[-1] = cm_eq(sp[-1], sp[0]); stack_p--; break;

		case kLexPower:
			sp[-1] = cm_pow(sp[-1], sp[0]);
			stack_p--;
			break;
		case kLexMin:           /* lower bound */
			if (cm_lt(sp[-1], sp[0]))
				sp[-1] = sp[0];
			stack_p--;
			break;
		case kLexMax:           /* upper bound */
			if (cm_lt(sp[0], sp[-1]))
				sp[-1] = sp[0];
			stack_p--;
			break;
		case kLexModulo:
			sp[-1] = cm_fmod(sp[-1], sp[0]);
			stack_p--;
			break;
		case kUnaryMinus:
			sp[0] = -sp[0];
			break;
		case kLexAbs:
			if (cm_lt(sp[0], 0.0))
				sp[0] = -sp[0];
			break;
		case kLexFun0:          /* random: 0 <= r < 1 */
			*(++stack_p) = (double)(uint16)qd_random() / 65536.0;
			break;
		case kLexFun1:
			switch (prog->value.token) {
			case kSinKey:   *sp = cm_sin(*sp);             break;
			case kCosKey:   *sp = cm_cos(*sp);             break;
			case kRoundKey: *sp = cm_floor(*sp + 0.5);     break;
			case kIntKey:   *sp = cm_floor(*sp);           break;
			}
			break;
		}
	}
	var->value = *(stack_p--);
	return var->value;
}

/* The script is TEXT 2128 (the prescript) followed by TEXT 2129, with a 0
   at the end, joined at build time as ASCR 128 and read in place, so it
   takes no dynamic heap. The original read an "Arashi Script" file in
   place of 2129 when there was one; that override is left out. */
SEG_SCRIPT static cbool load_program(void)
{
	uint32 size = 0;

	script = plat_resource(FOURCC('A', 'S', 'C', 'R'), 128, &size);
	if (script && (size == 0 || script[size - 1] != 0)) {
		plat_release(script);
		script = NULL;
	}
	return script != NULL;
}

SEG_SCRIPT void parser_load_level(int16 level)
{
	LexSymbol statement;

	if (!ready)
		return;
	current_level = level;

	if (from_level > current_level) {
		reset_variables();
		setup_compiler(script);
		lex_read(&pv.lookahead);
		from_level = -1;
		up_to_level = -1;
	}

	while (level >= up_to_level) {
		statement.kind = kParseError;
		parse_statement(&statement);

		if (pv.lookahead.kind == kParseError) {
			had_error = 1;
			up_to_level = 32000;
		} else if (statement.kind == kAssignment) {
			if (!tagbase_write(&program_base, statement.value.token,
			                   (uint32)pv.out_used * sizeof(LexSymbol), pv.out))
				had_error = 1;
		} else if (statement.kind == kLexLevel) {
			from_level = up_to_level;
			up_to_level = (int16)statement.value.level;
		}
		setup_compiler(pv.input);

		if (pv.lookahead.kind == kLexEof)
			up_to_level = 32000;
	}

	parser_write(kVarLevelNumber, (double)current_level);
}

SEG_SCRIPT cbool parser_alloc(void)
{
	if (ready)
		return 1;
	init_symbols();
	stack = (double *)plat_alloc(sizeof(double) * STACK_SIZE);
	stack_p = stack;
	if (!stack || !load_program()) {
		parser_free();
		return 0;
	}
	current_level = 0;
	from_level = 32000;
	up_to_level = -1;
	had_error = 0;
	ready = 1;
	return 1;
}

SEG_SCRIPT void parser_free(void)
{
	strdict_free(&sym_table);
	tagbase_free(&variable_base);
	tagbase_free(&program_base);
	if (stack)
		plat_free(stack);
	if (script)
		plat_release(script);
	if (pv.out)
		plat_free(pv.out);
	stack = stack_p = NULL;
	script = NULL;
	pv.out = NULL;
	pv.out_used = pv.out_cap = 0;
	ready = 0;
}

SEG_SCRIPT cbool parser_error(void)
{
	return had_error;
}

SEG_SCRIPT const uint8 *parser_token_name(int32 token, int16 *len)
{
	return strdict_word(&sym_table, (int16)token, len);
}
