#include <string.h>
#include <math.h>

#include "mpc.h"
#include "jblisp.h"

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_del(args); return lval_err(err); }

#define LASSERT_ARGC(fname, args, argc)                                        \
    if (args->count != argc) {                                                 \
        char msg[100];                                                         \
        snprintf(msg, (size_t) 100,                                            \
                 "Procedure '%s' expected %i argument(s), got %i.",            \
                 fname, argc, args->count);                                    \
        lval_del(args);                                                        \
        return lval_err(msg);                                                  \
    }

#define LASSERT_ARGT(fname, args, idx, exp_type)                               \
    if (args->val.cell[idx]->type != exp_type) {                               \
        char msg[100];                                                         \
        snprintf(msg, (size_t) 100,                                            \
                 "Procedure '%s' expected argument %i of type '%s', got '%s'.",\
                 fname, idx, TYPE_NAMES[exp_type],                             \
                 TYPE_NAMES[args->val.cell[idx]->type]);                       \
        lval_del(args);                                                        \
        return lval_err(msg);                                                  \
    }

// Lisp types
enum { LVAL_LNG, LVAL_DBL, LVAL_ERR, LVAL_SYM,
       LVAL_PROC, LVAL_SEXPR, LVAL_QEXPR };
char* TYPE_NAMES[] = {
    "integer", "decimal", "error", "symbol",
    "procedure", "s-expression", "q-expression"
};

struct lval {
    int type;
    int count;
    int size;
    union {
        double dbl;
        long lng;
        char *err;
        char *sym;
        struct lval **cell;
        lbuiltin proc;
    } val;
};

struct lenv {
    int count;
    int size;
    char **syms;
    lval **vals;
};

// Operator associativity types
enum { ASSOC_RIGHT, ASSOC_LEFT };

lenv *lenv_new(void) {
    lenv *e = malloc(sizeof(lenv));
    e->count = 0;
    e->size = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv *e) {
    for (int i=0; i < e->count; i++) {
        free(e->syms[i]);
        lval_del(e->vals[i]);
    }
}

void lenv_put(lenv *e, char *sym, lval *v) {
    int i = 0;
    int cmp;

    while((cmp = strcmp(e->syms[i], sym)) != 0 && i < e->count) {
        i++;
    }
    if (cmp == 0) {
        e->vals[i] = v;
    } else {
        e->count++;
        if (e->size == 0) {
            e->size = e->count * 2;
            e->syms = malloc(sizeof(char*) * e->size);
            e->vals = malloc(sizeof(lval*) * e->size);
        }
        else if (e->count > e->size) {
            e->size*=2;
            e->syms = realloc(e->syms, sizeof(char*) * e->size);
            e->vals = realloc(e->vals, sizeof(lval*) * e->size);
        }
        e->syms[e->count-1] = sym;
        e->vals[e->count-1] = v;
    }
}

lval *lenv_get(lenv *e, char *sym) {
    return (lval*) NULL;
}

lval *lenv_take(lenv *e, char *sym) {
    return NULL;
}

lval *lval_dbl(double x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_DBL;
    v->val.dbl = x;
    return v;
}

lval *lval_lng(long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_LNG;
    v->val.lng = x;
    return v;
}

lval *lval_err(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->val.err = malloc(strlen(m) + 1);
    strcpy(v->val.err, m);
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->val.cell = NULL;
    v->count = 0;
    v->size = 0;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->val.cell = NULL;
    v->count = 0;
    v->size = 0;
    return v;
}

lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->val.sym = malloc(sizeof(strlen(s) + 1));
    strcpy(v->val.sym, s);
    return v;
}

lval *lval_proc(lbuiltin proc) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_PROC;
    v->val.proc = proc;
    return v;
}

void lval_del(lval *v) {
    switch(v->type) {
        case LVAL_DBL:
            break;
        case LVAL_LNG:
            break;
        case LVAL_ERR:
            free(v->val.err);
            break;
        case LVAL_SYM:
            free(v->val.sym);
            break;
        case LVAL_PROC:
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            for (int i=0; i < v->count; i++)
                lval_del(v->val.cell[i]);
            free(v->val.cell);
            break;
    }
    free(v);
}

lval *lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_DBL:
            x->val.dbl = v->val.dbl;
            break;
        case LVAL_LNG:
            x->val.lng = v->val.lng;
            break;
        case LVAL_ERR:
            x->val.err = malloc(strlen(v->val.err) + 1);
            strcpy(x->val.err, v->val.err);
            break;
        case LVAL_SYM:
            x->val.sym = malloc(strlen(v->val.sym) + 1);
            strcpy(x->val.sym, v->val.sym);
            break;
        case LVAL_PROC:
            x->val.proc = v->val.proc;
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->val.cell = malloc(sizeof(lval*) * x->count);
            for (int i=0; i < v->count; i++) {
                x->val.cell[i] = lval_copy(v->val.cell[i]);
            }
            break;
    }
    return x;
}

lval *lval_insert(lval *x, lval *v, int n) {
    LASSERT(x, n <= x->count,
        "Array bounds error in INSERT.");
    x->count++;
    if (x->count >= x->size) {
        x->size = x->size == 0 ? x->count : x->size  *2;
        x->val.cell = realloc(x->val.cell, sizeof(lval*)  *x->size);
    }
    memmove(x->val.cell+n+1, x->val.cell+n, (x->count-n-1)  *sizeof(lval*));
    x->val.cell[n] = v;
    return x;
}

lval *lval_add(lval *x, lval *v) {
    return lval_insert(x, v, x->count);
}

lval *lval_pop(lval *v, int n) {
    lval *res = v->val.cell[n];
    memmove(v->val.cell+n, v->val.cell+n+1, (v->count-n-1)  *sizeof(lval*));
    v->count--;
    if (v->size > 2  *v->count) {
        v->val.cell = realloc(v->val.cell, sizeof(lval*)  *v->count);
        v->size = v->count;
    }
    return res;
}

// Take content of a cell, deleting the parent lval
lval *lval_take(lval *v, int n) {
    lval *res = lval_pop(v, n);
    lval_del(v);
    return res;
}

lval *lval_read_num(mpc_ast_t *ast) {
    errno = 0;
    if (strstr(ast->contents, ".") ||
        strstr(ast->contents, "e") ||
        strstr(ast->contents, "E"))
    {
        double x = strtod(ast->contents, NULL);
        return errno != ERANGE ? lval_dbl(x) : lval_err("invalid number");
    } else {
        long x = strtol(ast->contents, NULL, 10);
        return errno != ERANGE ? lval_lng(x) : lval_err("invalid number");
    }
}

lval *lval_read(mpc_ast_t *ast) {
    if (strstr(ast->tag, "number"))
        return lval_read_num(ast);
    if (strstr(ast->tag, "symbol"))
        return lval_sym(ast->contents);

    lval *x = NULL;
    if (strcmp(ast->tag, ">") == 0 || strstr(ast->tag, "sexpr"))
        x = lval_sexpr();
    else if (strstr(ast->tag, "qexpr"))
        x = lval_qexpr();
    else
        return lval_err("expected an S or Q expression");

    for (int i=0; i < ast->children_num; i++) {
        if (strcmp(ast->children[i]->contents, "(") == 0)
            continue;
        if (strcmp(ast->children[i]->contents, ")") == 0)
            continue;
        if (strcmp(ast->children[i]->contents, "{") == 0)
            continue;
        if (strcmp(ast->children[i]->contents, "}") == 0)
            continue;
        if (strcmp(ast->children[i]->tag, "regex") == 0)
            continue;
        x = lval_add(x, lval_read(ast->children[i]));
    }
    return x;
}

void lval_print_expr(lval *v, char open, char close) {
    putchar(open);
    for (int i=0; i < v->count; i++) {
        lval_print(v->val.cell[i]);
        if (i != v->count-1)
            putchar(' ');
    }
    putchar(close);
}

void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_LNG:
            printf("%li", v->val.lng);
            break;
        case LVAL_DBL:
            printf("%lf", v->val.dbl);
            break;
        case LVAL_SYM:
            printf("%s", v->val.sym);
            break;
        case LVAL_SEXPR:
            lval_print_expr(v, '(', ')');
            break;
        case LVAL_QEXPR:
            lval_print_expr(v, '{', '}');
            break;
        case LVAL_PROC:
            printf("<procedure at %p>", (void*) &v);
        case LVAL_ERR:
            printf("Error: %s", v->val.err);
            break;
        default:
            printf("Error: LVAL has invalid type.");
            break;
    }
}

void lval_println(lval *v) {
    lval_print(v);
    putchar('\n');
}

lval *lval_to_dbl(lval *v) {
    switch (v->type) {
        case LVAL_DBL:
            break;
        case LVAL_LNG:
            v->type = LVAL_DBL;
            v->val.dbl = (double) v->val.lng;
            break;
        default:
            lval_del(v);
            v = lval_err("type error: cannot be converted to number");
            break;
    }
    return v;
}

lval *builtin_op_dbl(lval *x, lval *y, char *op) {
    if (strcmp(op, "+") == 0) x->val.dbl += y->val.dbl;
    else if (strcmp(op, "-") == 0) x->val.dbl -= y->val.dbl;
    else if (strcmp(op, "*") == 0) x->val.dbl *= y->val.dbl;
    else if (strcmp(op, "/") == 0) x->val.dbl /= y->val.dbl;
    else if (strcmp(op, "^") == 0) x->val.dbl = pow(y->val.dbl, x->val.dbl);
    else if (strcmp(op, "min") == 0) {
        if (x->val.dbl > y->val.dbl) x->val.dbl = y->val.dbl;
    }
    else if (strcmp(op, "max") == 0) {
        if (x->val.dbl < y->val.dbl) x->val.dbl = y->val.dbl;
    }
    else if (strcmp(op, "and") == 0) {
        if (x->val.dbl != 0) x->val.dbl = y->val.dbl;
    }
    else if (strcmp(op, "or") == 0) {
        if (x->val.dbl == 0) x->val.dbl = y->val.dbl;
    }
    else {
        lval_del(x);
        x = lval_err("bad operator for type DOUBLE");
    }
    lval_del(y);
    return x;
}

lval *builtin_op_lng(lval *x, lval *y, char *op) {
    if (strcmp(op, "+") == 0) x->val.lng += y->val.lng;
    else if (strcmp(op, "-") == 0) x->val.lng -= y->val.lng;
    else if (strcmp(op, "*") == 0) x->val.lng *= y->val.lng;
    else if (strcmp(op, "/") == 0) x->val.lng /= y->val.lng;
    else if (strcmp(op, "^") == 0) x->val.lng = pow(y->val.lng, x->val.lng);
    else if (strcmp(op, "min") == 0) {
        if (x->val.lng > y->val.lng) x->val.lng = y->val.lng;
    }
    else if (strcmp(op, "max") == 0) {
        if (x->val.lng < y->val.lng) x->val.lng = y->val.lng;
    }
    else if (strcmp(op, "and") == 0) {
        if (x->val.lng != 0) x->val.lng = y->val.lng;
    }
    else if (strcmp(op, "or") == 0) {
        if (x->val.lng == 0) x->val.lng = y->val.lng;
    }
    else if (strcmp(op, "%") == 0) x->val.lng %= y->val.lng;
    else {
        lval_del(x);
        y = lval_err("bad operator for type LONG");
    }
    lval_del(y);
    return x;
}

lval *builtin_op(lval *a, char *op) {
    if (a->type == LVAL_ERR) return a;

    // If any operand is a double, cast all to double
    int type = LVAL_LNG;
    for (int i=0; i < a->count; i++) {
        if (a->val.cell[i]->type == LVAL_DBL)
            type = LVAL_DBL;
        else if (a->val.cell[i]->type != LVAL_LNG) {
            lval_del(a);
            return lval_err("cannot operate on non-number.");
        }
    }
    if (type == LVAL_DBL) {
        for (int i=0; i < a->count; i++) {
            a->val.cell[i] = lval_to_dbl(a->val.cell[i]);
        }
    }

    // Set associativity rule
    int assoc;
    if (strcmp(op, "^") == 0) assoc = ASSOC_RIGHT;
    else                      assoc = ASSOC_LEFT;

    lval *x;
    if (assoc == ASSOC_LEFT) x = lval_pop(a, 0);
    else                     x = lval_pop(a, a->count-1);

    // (- x) = -x
    if ((strcmp(op, "-") == 0) && a->count == 0) {
        switch (type) {
            case LVAL_DBL:
                x = builtin_op_dbl(lval_dbl(0.0), x, op);
                break;
            case LVAL_LNG:
                x = builtin_op_lng(lval_lng(0.0), x , op);
                break;
        }
    }

    while (a->count > 0) {
        lval *y;
        if (assoc == ASSOC_LEFT) y = lval_pop(a, 0);
        else                     y = lval_pop(a, a->count-1);

        switch (type) {
            case LVAL_DBL:
                x = builtin_op_dbl(x, y, op);
                break;
            case LVAL_LNG:
                x = builtin_op_lng(x, y, op);
                break;
        }
        if (x->type == LVAL_ERR) break;
    }
    lval_del(a);
    return x;
}

lval *builtin_nth(lval *a) {
    LASSERT_ARGC("nth", a, 2)
    LASSERT_ARGT("nth", a, 0, LVAL_QEXPR);
    LASSERT_ARGT("nth", a, 1, LVAL_LNG);
    LASSERT(a, a->val.cell[0]->count != 0,
        "Procedure 'nth' undefined on empty list '{}'.");

    int n = (int) a->val.cell[1]->val.lng;
    if (n < 0)
        n = a->val.cell[0]->count + n;

    LASSERT(a, n >= 0 ,
        "Array bounds error at procedure 'nth'.");
    LASSERT(a, n < a->val.cell[0]->count,
        "Array bounds error at procedure 'nth'.");

    // Delete all but first cell
    lval *v = lval_take(a, 0);
    return lval_take(v, n);
}

lval *builtin_car(lval *a) {
    LASSERT_ARGC("car", a, 1);
    LASSERT_ARGT("car", a, 0, LVAL_QEXPR);
    LASSERT(a, a->val.cell[0]->count != 0,
        "Procedure 'car' undefined on empty list '{}'.");

    // Delete all but first cell
    lval *v = lval_take(a, 0);
    return lval_take(v, 0);
}

lval *builtin_cdr(lval *a) {
    LASSERT_ARGC("cdr", a, 1)
    LASSERT(a, a->val.cell[0]->type == LVAL_QEXPR,
        "Type Error - procedure 'cdr' expected a Q expression.");
    LASSERT(a, a->val.cell[0]->count != 0,
        "Procedure 'cdr' undefined on empty list '{}'.");

    //Delete head
    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval *builtin_list(lval *a) {
    LASSERT(a, a->type == LVAL_SEXPR,
        "Procedure 'list' expected an S expression");
    a->type = LVAL_QEXPR;
    return a;
}

lval *builtin_eval(lval *a) {
    LASSERT_ARGC("eval", a, 1);
    LASSERT(a, a->val.cell[0]->type == LVAL_QEXPR,
        "Procedure 'eval' expected a Q expression");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval *builtin_join(lval *a) {
    LASSERT(a, a->count != 0,
        "Procedure 'join' takes at least 1 argument.");
    for (int i=0; i < a->count; i++) {
        LASSERT(a, a->val.cell[i]->type == LVAL_QEXPR,
            "Procedure 'join' takes only Q expression arguments.");
    }

    lval *x = lval_pop(a, 0);
    while (a->count) {
        lval *y = lval_pop(a, 0);
        while (y->count)
            x = lval_add(x, lval_pop(y, 0));
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval *builtin_cons(lval *a) {
    LASSERT_ARGC("cons", a, 2);
    LASSERT(a, a->val.cell[1]->type == LVAL_QEXPR,
        "Second argument to 'cons' must be a Q expression.");
    lval *x = lval_pop(a, 0);
    lval *q = lval_take(a, 0);
    q = lval_insert(q, x, 0);
    return q;
}

lval *builtin_len(lval *a) {
    LASSERT_ARGC("len", a, 1);
    LASSERT(a, a->val.cell[0]->type == LVAL_QEXPR,
        "Procedure 'len' only applies to Q expressions.");
    lval *x = lval_lng(a->val.cell[0]->count);
    lval_del(a);
    return x;
}

lval *builtin_init(lval *a) {
    LASSERT_ARGC("init", a, 1);
    LASSERT(a, a->val.cell[0]->type == LVAL_QEXPR,
        "Procedure 'init' only applies to Q expressions.");

    // Keep all but first cell
    lval *x = lval_take(a, 0);
    lval_del(lval_pop(x, x->count-1));
    return x;
}

lval *builtin_last(lval *a) {
    LASSERT_ARGC("last", a, 1);
    LASSERT(a, a->val.cell[0]->type == LVAL_QEXPR,
        "Procedure 'last' only applies to Q expressions.");

    lval *x = lval_take(a, 0);
    return lval_take(x, x->count-1);
}

// cadr, cdar, caar, etc.
lval *builtin_c__r(lval *a, char *func) {
    char err_msg[100];
    snprintf(err_msg, 100, "Unknown builtin procedure '%s'.", func);
    int len = strlen(func);
    LASSERT(a, len > 2, err_msg);
    LASSERT(a, func[0] == 'c' && func[len-1] == 'r', err_msg);

    for (int i = len-2; i > 0; i--) {
        if (func[i] == 'a')
            a = builtin_car(a);
        else if (func[i] == 'd')
            a = builtin_cdr(a);
        else {
            lval_del(a);
            return lval_err(err_msg);
        }
        lval *x = lval_sexpr();
        a = lval_add(x, a);
    }
    return lval_take(a, 0);
}

lval *builtin(lval *a, char *func) {
    if (strcmp("list", func) == 0) return builtin_list(a);
    if (strcmp("join", func) == 0) return builtin_join(a);
    if (strcmp("eval", func) == 0) return builtin_eval(a);
    if (strcmp("cons", func) == 0) return builtin_cons(a);
    if (strcmp("car", func) == 0) return builtin_car(a);
    if (strcmp("cdr", func) == 0) return builtin_cdr(a);
    if (strcmp("nth", func) == 0) return builtin_nth(a);
    if (strcmp("len", func) == 0) return builtin_len(a);
    if (strcmp("init", func) == 0) return builtin_init(a);
    if (strcmp("last", func) == 0) return builtin_last(a);
    if (strcmp("and", func) == 0) return builtin_op(a, func);
    if (strcmp("or", func) == 0) return builtin_op(a, func);
    if (strcmp("not", func) == 0) return builtin_op(a, func);
    if (strcmp("min", func) == 0) return builtin_op(a, func);
    if (strcmp("max", func) == 0) return builtin_op(a, func);
    if (strstr("^+-/*%", func)) return builtin_op(a, func);
    return builtin_c__r(a, func);
}

lval *lval_eval_sexpr(lval *v) {
    for (int i=0; i < v->count; i++)
        v->val.cell[i] = lval_eval(v->val.cell[i]);

    if (v->count == 0)
        return v;

    lval *op = lval_pop(v, 0);
    if (op->type != LVAL_SYM) {
        lval_del(op);
        lval_del(v);
        return lval_err("Expected procedure at start of S expression.");
    }

    lval *result = builtin(v, op->val.sym);
    lval_del(op);
    return result;
}

lval *lval_eval(lval *v) {
    if (v->type == LVAL_SEXPR)
        return lval_eval_sexpr(v);
    else
        return v;
}

void exec_file(char *filename) {
    mpc_result_t res;

    printf("Loading file '%s'...", filename);
    if (mpc_parse_contents(filename, JBLisp, &res)) {
        lval *prog = lval_read(res.output);
        mpc_ast_delete(res.output);
        while (prog->count) {
            lval *x = lval_eval(lval_pop(prog, 0));
            if (x->type == LVAL_ERR) {
                lval_println(x);
            }
            lval_del(x);
        }
        lval_del(prog);
    } else {
        mpc_err_print(res.error);
        mpc_err_delete(res.error);
    }
    puts("done.");
}

void exec_line(char *input) {
    mpc_result_t res;

    if (mpc_parse("<stdin>", input, JBLisp, &res)) {
        lval *line = lval_read(res.output);
        mpc_ast_delete(res.output);
        while (line->count) {
            lval *x = lval_eval(lval_pop(line, 0));
            lval_println(x);
            lval_del(x);
        }
        lval_del(line);
    } else {
        mpc_err_print(res.error);
        mpc_err_delete(res.error);
    }
}

void build_parser() {
    Number    = mpc_new("number");
    Symbol    = mpc_new("symbol");
    Sexpr     = mpc_new("sexpr");
    Qexpr     = mpc_new("qexpr");
    Expr      = mpc_new("expr");
    JBLisp    = mpc_new("jblisp");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                     \
            number   : /((-?[0-9]*[.][0-9]+)|(-?[0-9]+[.]?))([eE]-?[0-9]+)?/ ;\
            symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                     \
            sexpr    : '(' <expr>* ')' ;                                      \
            qexpr    : '{' <expr>* '}' ;                                      \
            expr     : <number> | <symbol> | <sexpr> | <qexpr> ;              \
            jblisp   : /^/ <expr>* /$/ ;                                      \
        ",
        Number, Symbol, Sexpr, Qexpr, Expr, JBLisp
    );
}
