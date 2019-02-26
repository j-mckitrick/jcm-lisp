/*
 * JCM-LISP
 *
 * Based on http://web.sonoma.edu/users/l/luvisi/sl3.c
 * and http://peter.michaux.ca/archives
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/errno.h>

#define MAX_BUFFER_SIZE 100

#define MAX_ALLOC_SIZE  256

#define GC_ENABLED
#define GC_MARK
#define GC_SWEEP
#define GC_DEBUG
//#define GC_DEBUG_X
//#define GC_PIN

//#define CODE_TEST
//#define FILE_TEST
#define REPL

typedef enum {
  UNKNOWN = 0,
  NIL = 1,
  FIXNUM = 2,
  STRING = 3,
  SYMBOL = 4,
  CELL = 5,
  PRIMITIVE = 6,
  PROC = 7
} obj_type;

typedef struct Object Object;
typedef struct Object *(primitive_fn)(struct Object *);

struct Fixnum {
  int value;
};

struct String {
  char *text;
};

struct Symbol {
  char *name;
};

struct Cell {
  struct Object *car;
  struct Object *cdr;
};

struct Primitive {
  primitive_fn *fn;
};

struct Proc {
  struct Object *vars;
  struct Object *body;
  struct Object *env;
};

struct Object {
  obj_type type;

  union {
    struct Fixnum num;
    struct String str;
    struct Symbol symbol;
    struct Cell cell;
    struct Primitive primitive;
    struct Proc proc;
  };

  int mark;
  int id;
  struct Object *next;
};

void print(Object *);

Object *symbols;    /* linked list of symbols */
Object *s_quote;
Object *s_define;
Object *s_setq;
Object *s_nil;
Object *s_if;
Object *s_t;

Object *s_lambda;

Object *free_list[MAX_ALLOC_SIZE];
Object *active_list[MAX_ALLOC_SIZE];
Object *env;

int current_mark;

void error(char *msg) {
  printf("\nError %s\n", msg);
  exit(0);
}

#ifdef GC_PIN
struct PinnedVariable {
  Object **variable;
  struct PinnedVariable *next;
};

struct PinnedVariable *pinned_variables;

void pin_variable(Object **obj) {

#ifdef GC_DEBUG_X
  printf("> variable %p\n", obj);
#endif

  struct PinnedVariable *pinned_var = calloc(1, sizeof(struct PinnedVariable));
  assert(pinned_var != NULL);
  pinned_var->variable = obj;

  pinned_var->next = pinned_variables;

#ifdef GC_DEBUG_X
  printf("Pinned variable         = %p\n", pinned_var);
  //printf("Pinned variable         = ");
  //print((struct Object *)pinned_var->variable);
  printf("Pinned variables before = %p\n", pinned_variables);
#endif

  pinned_variables = pinned_var;

#ifdef GC_DEBUG_X
  printf("Pinned variables after  = %p\n", pinned_variables);
#endif
}
#else
void pin_variable(Object **obj) { }
#endif // GC_PIN

#ifdef GC_PIN
void unpin_variable(Object **variable) {

#ifdef GC_DEBUG_X
  printf("< variable %p\n", variable);
#endif

  struct PinnedVariable **v;
  for (v = &pinned_variables; *v != NULL; v = &(*v)->next) {
    struct PinnedVariable *target = *v;

    if (target->variable == variable) {

#ifdef GC_DEBUG_X
      printf("Pinned variable found %p\n", *v);
#endif
      struct PinnedVariable *next = target->next;
      free(target);
      target = next;
#ifdef GC_DEBUG_X
      printf("Pinned variable  head %p\n", *v);
#endif
      return;
    }
  }
  assert(0);
}
#else
void unpin_variable(Object **variable) { }
#endif // GC_PIN

char *get_type(Object *obj) {
  if (obj == NULL)
    return "NULL";
  else if (obj->type == FIXNUM)
    return "FIXNUM";
  else if (obj->type == STRING)
    return "STRING";
  else if (obj->type == SYMBOL)
    return "SYMBOL";
  else if (obj->type == CELL)
    return "CELL";
  else if (obj->type == PRIMITIVE)
    return "PRIM";
  else if (obj->type == PROC)
    return "PROC";
  else
    return "UNKNOWN";
}

int is_fixnum(Object *obj) {
  return (obj && obj->type == FIXNUM);
}

int is_string(Object *obj) {
  return (obj && obj->type == STRING);
}

int is_symbol(Object *obj) {
  return (obj && obj->type == SYMBOL);
}

int is_cell(Object *obj) {
  return (obj && obj->type == CELL);
}

int is_primitive(Object *obj) {
  return (obj && obj->type == PRIMITIVE);
}

int is_proc(Object *obj) {
  return (obj && obj->type == PROC);
}

Object *car(Object *obj) {
  if (is_cell(obj))
    return obj->cell.car;
  else
    return s_nil;
}

Object *cdr(Object *obj) {
  if (is_cell(obj))
    return obj->cell.cdr;
  else
    return s_nil;
}

#define caar(obj)    car(car(obj))
#define cadr(obj)    car(cdr(obj))

void setcar(Object *obj, Object *val) {
  obj->cell.car = val;
}

void setcdr(Object *obj, Object *val) {
  if (obj == NULL)
    error("Cannot set NULL cdr");

  if (obj->cell.cdr != NULL && obj->cell.cdr != s_nil)
    printf("Changing %p -> cdr %p to %p\n", obj, obj->cell.cdr, val);

  obj->cell.cdr = val;
}

#ifdef GC_ENABLED
void mark(Object *obj) {
  if (obj == NULL) {
#ifdef GC_DEBUG
    printf("\nNothing to mark: NULL");
#endif
    return;
  }
#ifdef GC_DEBUG
  Object *temp = obj; printf("\nMarking %p\n", temp);
#endif
  if (obj->mark > 0) {
#ifdef GC_DEBUG
    printf("Mark:\n");
    printf("\nNothing to mark: already marked\n");
    //print(obj);
#endif
    return;
  }

  //sleep(1);

#ifdef GC_DEBUG
  //printf("\nBefore mark: %p %d", obj, obj->mark);
#endif

  /* if (obj->mark > current_mark + 10) { */
  /*   printf("\nToo many marks:\n"); */
  /*   print(obj); */
  /*   exit(-1); */
  /* } */

  //obj->mark++;
  //obj->mark = 1;
  obj->mark = current_mark;

#ifdef GC_DEBUG
  //printf("\nAfter mark: %p %d", obj, obj->mark);
  if (obj->id >= 4 && obj->id <= 5) {
    //printf("TARGET! v\n");
    //print(obj);
    //printf("TARGET! ^\n");
  }
#endif

  switch (obj->type) {
    case FIXNUM:
    case STRING:
    case SYMBOL:
    case PRIMITIVE:
#ifdef GC_DEBUG
      printf("\nMark %d %s ", obj->id, get_type(obj));
      print(obj);
#endif
      break;
    case CELL:
#ifdef GC_DEBUG
      printf("\nMark cell car %p -> %p", obj, obj->cell.car);
#endif
      mark(obj->cell.car);
#ifdef GC_DEBUG
      printf("\nMark cell car %p <-", obj);
#endif
#ifdef GC_DEBUG
      printf("\nMark cell cdr %p -> %p", obj, obj->cell.cdr);
#endif
      mark(obj->cell.cdr);
#ifdef GC_DEBUG
      printf("\nMark cell cdr %p <-", obj);
#endif
      break;
    case PROC:
#ifdef GC_DEBUG
      printf("\nMark proc ->");
      //print(obj);
#endif
#ifdef GC_DEBUG
      printf("\nMark proc vars");
#endif
      mark(obj->proc.vars);
#ifdef GC_DEBUG
      printf("\nMark proc body");
#endif
      mark(obj->proc.body);
#ifdef GC_DEBUG
      printf("\nMark proc env");
#endif
      mark(obj->proc.env);
#ifdef GC_DEBUG
      printf("\nMark proc <-");
#endif
      break;
    default:
      printf("\nMark unknown object: %d\n", obj->type);
      break;
  }
}

int is_active(Object *needle) {
  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    if (active_list[i] == needle)
      return 1;
  }

  return 0;
}

void sweep() {
  int counted = 0, kept = 0, swept = 0;
  int cells = 0;

  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    Object *obj = active_list[i];

    if (obj == NULL)
      continue;

#ifdef GC_DEBUG
    //printf("\nObj at                 = %p\n", obj);
#endif

    if (obj->mark == 0) {
      //printf("obj->mark: %d\n", obj->mark);
      printf("SWEEP: %p %d ", obj, obj->id);
      print(obj);

      // Free any additional allocated memory.
      switch (obj->type) {
        case STRING:
          memset(obj->str.text, 0, strlen(obj->str.text));
          free(obj->str.text);
          printf("\n");
          break;
        case SYMBOL:
          memset(obj->symbol.name, 0, strlen(obj->symbol.name));
          free(obj->symbol.name);
          printf("\n");
          break;
        case CELL:
          cells++;
          break;
        default:
          printf("\n");
          break;
      }

      free_list[i] = obj;
      active_list[i] = NULL;
      swept++;

    } else {
#ifdef GC_DEBUG
      /* printf("Mark it ZERO! %d ", obj->mark); */
      printf("Do NOT sweep: %p %d ", obj, obj->id);
      print(obj);
      switch (obj->type) {
        case STRING:
          break;
        case SYMBOL:
          printf("\n");
          break;
        case PROC:
          printf("\n");
          break;
        case PRIMITIVE:
          printf("\n");
          break;
        default:
          //printf("\n");
          break;
      }
      if (!is_active(obj)) {
        error("NOT found in active list!\n");
      }
#endif
      obj->mark = 0;
      kept++;
    }

    counted++;
  }
#ifdef GC_DEBUG
  printf("Done sweep: %d kept %d swept %d counted\n\n", kept, swept, counted);
  printf("%d cells\n", cells);
#endif
}

int check_active() {
  int counted = 0;

  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    if (active_list[i] != NULL)
      counted++;
  }

#ifdef GC_DEBUG
  printf("Done check_active: %d counted\n\n", counted);
#endif
  return counted;
}

int check_free() {
  int counted = 0;

  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    if (free_list[i] != NULL)
      counted++;
  }

#ifdef GC_DEBUG
  printf("Done check_free: %d counted\n\n", counted);
#endif
  return counted;
}

void check_mem() {
  int active = check_active();
  int free = check_free();
  int total = active + free;
  printf("\nDone check_mem: %d total\n", total);
  if (total != MAX_ALLOC_SIZE) {
    printf("Missing %d objects", MAX_ALLOC_SIZE - total);
    error("check_mem fail!");
  }
}

void gc() {

  printf("\nGC v----------------------------------------v\n");
  //check_mem();

#ifdef GC_MARK
  /* current_mark++; */
  /* printf("Current mark: %d\n", current_mark); */

  printf("\n-------- Mark symbols:");
  mark(symbols);

  printf("\n-------- Mark env:");
  mark(env);

#ifdef GC_PIN
  printf("\n-------- Mark pins:\n");
  struct PinnedVariable **pv = &pinned_variables;
  printf("pinned_variables = %p\n", pinned_variables);
  printf("pinned_variables address = %p\n", pv);
  while (*pv != NULL) {
    printf("pinned_variable = %p\n", *pv);
    print((struct Object *)(*pv)->variable);
    //((struct Object *)(*pv)->variable)->mark = current_mark;
    ((struct Object *)(*pv)->variable)->mark = 99;
    pv = &(*pv)->next;
  }
#endif
  /* while (*pv != NULL) { */
  /*   (*(*pv)->variable)->mark = current_mark; */
  /*   pv = &(*pv)->next; */
  /* } */
#endif

#ifdef GC_SWEEP
  printf("\n-------- Sweep\n");
  sweep();
  //check_mem();
#endif

  printf("\nGC ^----------------------------------------^\n");
}

Object *find_next_free() {
  Object *obj = NULL;

  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    obj = free_list[i];

    if (obj != NULL) {
      free_list[i] = NULL;
      active_list[i] = obj;
      break;
    }
  }

  return obj;
}

#endif // GC_ENABLED

Object *alloc_Object() {
  Object *obj = find_next_free();

  if (obj == NULL) {
    gc();

    obj = find_next_free();

    if (obj == NULL) {
      printf("Out of memory\n");
      exit(-1);
    }
  }

  return obj;
}

Object *new_Object() {
#ifdef GC_ENABLED
  Object *obj = alloc_Object();
#else
  Object *obj = calloc(1, sizeof(Object));
#endif

  obj->type = UNKNOWN;
  obj->mark = 0;
  return obj;
}

Object *make_cell() {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();

  obj->type = CELL;
  obj->cell.car = s_nil;
  obj->cell.cdr = s_nil;
  unpin_variable(&obj);
  return obj;
}

Object *cons(Object *car, Object *cdr) {
  Object *obj;

  pin_variable(&obj);
  obj = make_cell();
  obj->cell.car = car;
  obj->cell.cdr = cdr;
  unpin_variable(&obj);
  return obj;
}

Object *make_string(char *str) {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();
  obj->type = STRING;
  obj->str.text = strdup(str);
  unpin_variable(&obj);
  return obj;
}

Object *make_fixnum(int n) {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();
  obj->type = FIXNUM;
  obj->num.value = n;
  unpin_variable(&obj);
  return obj;
}

Object *make_symbol(char *name) {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();
  obj->type = SYMBOL;
  obj->symbol.name = strdup(name);
  unpin_variable(&obj);
  return obj;
}

Object *make_primitive(primitive_fn *fn) {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();
  obj->type = PRIMITIVE;
  obj->primitive.fn = fn;
  unpin_variable(&obj);
  return obj;
}

Object *make_proc(Object *vars, Object *body, Object *env) {
  Object *obj;

  pin_variable(&obj);
  obj = new_Object();
  obj->type = PROC;
  obj->proc.vars = vars;
  obj->proc.body = body;
  obj->proc.env = env;
  unpin_variable(&obj);
  return obj;
}

/* Walk simple linked list of symbols
 * for one matching name.
 */
Object *lookup_symbol(char *name) {
  Object *cell = symbols;
  Object *sym;

  //printf("Symbol for lookup %s\n", name);

  while (cell != s_nil) {
    sym = car(cell);

    //printf("Symbol for lookup comparison? %d %s\n", is_symbol(sym), sym->symbol.name);

    if (is_symbol(sym) &&
        strcmp(sym->symbol.name, name) == 0) {
      //printf("Symbol lookup succeeded\n");
      //printf("Symbol address %p\n", sym);
      return sym;
    }

    cell = cdr(cell);
  }

  return NULL;
}

/* Lookup a symbol, and return it if found.
 * If not found, create a new one and
 * add it to linked list of symbols,
 * and return the new symbol.
 */
Object *intern_symbol(char *name) {
  Object *sym = lookup_symbol(name);

  if (sym == NULL) {
    printf("Make symbol %s\n", name);
    sym = make_symbol(name);
    printf("Made symbol %p\n", sym);
    symbols = cons(sym, symbols);
  }

  return sym;
}

// BUG: Does not walk list of environments!
Object *assoc(Object *key, Object *list) {
  if (list != s_nil) {
    Object *pair = car(list);
    //printf("\nAssoc check %s\n", car(pair)->symbol.name);
    if (car(pair) == key)
      return pair;

    return assoc(key, cdr(list));
  }

  return NULL;
}

Object *primitive_add(Object *args) {
  int total = 0;

  while (args != s_nil) {
    total += car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(total);
}

Object *primitive_sub(Object *args) {
  long result = car(args)->num.value;

  args = cdr(args);
  while (args != s_nil) {
    result -= car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(result);
}

Object *primitive_mul(Object *args) {
  long total = 1;

  while (args != s_nil) {
    total *= car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(total);
}

Object *primitive_div(Object *args) {
  long dividend = car(args)->num.value;
  long divisor = cadr(args)->num.value;
  long quotient = 0;

  if (divisor != 0)
    quotient = dividend / divisor;

  return make_fixnum(quotient);
}

int is_whitespace(char c) {
  if (isspace(c))
    return 1;
  else
    return 0;
}

int is_symbol_char(char c) {
  return (isalnum(c) ||
          strchr("+-*/", c));
}

void skip_whitespace(FILE *in) {
  char c;
  int done = 0;

  while (!done) {
    c = getc(in);
    if (c == '\n' && c == '\r')
      done = 1;

    if (!is_whitespace(c)) {
      ungetc(c, in);
      done = 1;
    }
  }
}

Object *read_string(FILE *in) {
  char buffer[MAX_BUFFER_SIZE];
  int i = 0;
  char c;

  while ((c = getc(in)) != '"' &&
         i < MAX_BUFFER_SIZE - 1) {
    buffer[i++] = c;
  }

  buffer[i] = '\0';
  return make_string(buffer);
}

Object *read_symbol(FILE *in) {
  char buffer[MAX_BUFFER_SIZE];
  int i = 0;
  char c;

  while (!is_whitespace(c = getc(in)) &&
         is_symbol_char(c) &&
         i < MAX_BUFFER_SIZE - 1) {
    buffer[i++] = c;
  }

  buffer[i] = '\0';
  ungetc(c, in);

  Object *obj = intern_symbol(buffer);

  return obj;
}

Object *read_number(FILE *in) {
  int number = 0;
  char c;

  while (isdigit(c = getc(in))) {
    number *= 10;
    number += (int)c - 48;
  }

  ungetc(c, in);

  return make_fixnum(number);
}

Object *read_list(FILE *);

// XXX Review these: strchr, strdup, strcmp, strspn, atoi
Object *read_lisp(FILE *in) {
  Object *obj = s_nil;
  char c;

  skip_whitespace(in);
  c = getc(in);

  if (c == '\'') {
    obj = cons(s_quote, cons(read_lisp(in), s_nil));
  } else if (c == '(') {
    obj = read_list(in);
  } else if (c == '"') {
    obj = read_string(in);
  } else if (isdigit(c)) {
    ungetc(c, in);
    obj = read_number(in);
  } else if (isalpha(c)) {
    ungetc(c, in);
    obj = read_symbol(in);
  } else if (strchr("+-/*", c)) {
    ungetc(c, in);
    obj = read_symbol(in);
  } else if (c == ')') {
    ungetc(c, in);
  } else if (c == EOF) {
#ifdef REPL
    exit(0);
#else
    return NULL;
#endif
  }

  return obj;
}

Object *read_list(FILE *in) {
  char c;
  Object *car, *cdr;

  car = cdr = make_cell();
  car->cell.car = read_lisp(in);

  while ((c = getc(in)) != ')') {
    if (c == '.') {
      // Discard the char after '.'
      // but we should check for whitespace.
      getc(in);

      // The rest goes into the cdr.
      cdr->cell.cdr = read_lisp(in);
    } else if (!is_whitespace(c)) {
      ungetc(c, in);

      cdr->cell.cdr = make_cell();
      cdr = cdr->cell.cdr;
      cdr->cell.car = read_lisp(in);
    }
  }

  return car;
}

/*
 * Returns a list with a new cons cell
 * containing var and val at the head
 * and the existing env in the tail.
 */
Object *extend(Object *env, Object *var, Object *val) {
  /* printf("Extend list %p with var ", env); */
  /* print(var); */
  /* printf(" = "); */
  /* print(val); */
  /* printf("\n"); */

  Object *pair = cons(var, val);

  return cons(pair, env);
}

/*
 * Set the tail of this env to a new list
 * with var and val at the head.
 */
Object *extend_env(Object* env, Object *var, Object *val) {
  Object *current_env = cdr(env);
  /* printf("Current env = "); */
  /* print(env); */
  /* printf("Extend env %p with var ", env); */
  /* print(var); */
  /* printf(" = "); */
  /* print(val); */
  /* printf("\n"); */

  Object *updated_env = extend(current_env, var, val);

  setcdr(env, updated_env);

  return val;
}

Object *eval(Object *obj, Object *env);

/* Return list of evaluated args. */
Object *eval_args(Object *args, Object *env) {
  if (args != s_nil)
    return cons(eval(car(args), env), eval_args(cdr(args), env));

  return s_nil;
}

Object *progn(Object *forms, Object *env) {
  if (forms == s_nil)
    return s_nil;

  for (;;) {
    if (cdr(forms) == s_nil)
      return eval(car(forms), env);

    forms = cdr(forms);
  }

  return s_nil;
}

/* Iterate vars and vals, adding each pair to this env. */
Object *multiple_extend_env(Object *env, Object *vars, Object *vals) {
  if (vars == s_nil)
    return env;
  else
    return multiple_extend_env(extend(env, car(vars), car(vals)), cdr(vars), cdr(vals));
}

Object *apply(Object *proc, Object *args, Object *env) {
  if (is_primitive(proc))
    return (*proc->primitive.fn)(args);

  if (is_proc(proc))
    return progn(proc->proc.body, multiple_extend_env(env, proc->proc.vars, args));

  printf("Dumping %s\n", get_type(proc));
  print(proc);
  /* print(proc->proc.vars); */
  /* print(proc->proc.body); */
  /* print(proc->proc.env); */
  error("Bad apply");

  return s_nil;
}

Object *eval_symbol(Object *obj, Object *env) {
  /* printf("Symbol %p\n", obj); */
  /* printf("Symbol name '%s'\n", obj->symbol.name); */

  Object *pair = assoc(obj, env);

  if (pair == NULL) {
    char *buff = NULL;
    asprintf(&buff, "Undefined symbol '%s'", obj->symbol.name);
    error(buff);
  }
  /* printf("Pair   = %p\n", pair); */
  /* printf("Symbol = %p\n", cdr(pair)); */

  return cdr(pair);
}

Object *eval_list(Object *obj, Object *env) {
  if (obj == s_nil)
    return obj;

  //printf("Eval list\n");

  if (car(obj) == s_define) {
    Object *cell = obj; // car(cell) should be symbol named define

    cell = cdr(cell);
    Object *cell_symbol = car(cell);

    cell = cdr(cell);
    Object *cell_value = car(cell);

    Object *var = cell_symbol;
    Object *val = eval(cell_value, env);

    // Check for existing binding?
    Object *pair = assoc(cell_symbol, env);

    if (pair == NULL) {
      return extend_env(env, var, val);
    } else {
      setcdr(pair, val);

      return val;
    }
  } else if (car(obj) == s_setq) {
    Object *cell = obj; // car(cell) should be symbol named setq

    cell = cdr(cell);
    Object *cell_symbol = car(cell);

    cell = cdr(cell);
    Object *cell_value = car(cell);

    Object *pair = assoc(cell_symbol, env);
    Object *newval = eval(cell_value, env);

    setcdr(pair, newval);

    return newval;
  } else if (car(obj) == s_if) {
    Object *cell = obj;

    cell = cdr(cell);
    Object *cell_condition = car(cell);

    cell = cdr(cell);
    Object *cell_true_branch = car(cell);

    cell = cdr(cell);
    Object *cell_false_branch = car(cell);

    if (eval(cell_condition, env) != s_nil)
      return eval(cell_true_branch, env);
    else
      return eval(cell_false_branch, env);

  } else if (car(obj) == s_quote) {
    return car(cdr(obj));
  } else if (car(obj) == s_lambda) {
    Object *vars = car(cdr(obj));
    Object *body = cdr(cdr(obj));

    return make_proc(vars, body, env);
  }

  /* This list is not a builtin, so treat it as a function call. */
  Object *proc = eval(car(obj), env);
  Object *args = eval_args(cdr(obj), env);

  return apply(proc, args, env);
}

Object *eval(Object *obj, Object *env) {
  if (obj == NULL)
    return obj;

  Object *result = s_nil;

  //printf("Eval:\n");

  switch (obj->type) {
    case STRING:
    case FIXNUM:
    case PRIMITIVE:
    case PROC:
      result = obj;
      break;
    case SYMBOL:
      result = eval_symbol(obj, env);
      break;
    case CELL:
      result = eval_list(obj, env);
      break;
    default:
      printf("\nUnknown Object: %d\n", obj->type);
      break;
  }

  return result;
}

void print_string(Object *obj) {
  char *str = obj->str.text;
  int len = strlen(str);
  int i = 0;

  putchar('"');
  while (i < len) {
    putc(*str++, stdout);
    i++;
  }

  putchar('"');
}

void print_fixnum(Object *obj) {
  //printf("Fixnum\n");
  printf("%d", obj->num.value);
}

void print_cell(Object *car) {
  Object *obj = car;

  printf("(");

  while (obj != s_nil && obj != NULL) {
    if (obj->type == CELL) {
      print(obj->cell.car);
    } else {
      printf(". ");
      print(obj);
      break;
    }

    //sleep(1);
    if (obj == obj->cell.cdr)
      error("Circular reference??");

    obj = obj->cell.cdr;

    if (obj != s_nil && obj != NULL)
      printf(" ");
  }

  printf(")\n");
}

void print(Object *obj) {
  if (obj == NULL) {
    printf("NULL ERROR\n");
    return;
  }

  switch (obj->type) {
    case FIXNUM:
      print_fixnum(obj);
      break;
    case STRING:
      print_string(obj);
      break;
    case SYMBOL:
      printf("%s", obj->symbol.name);
      break;
    case CELL:
      print_cell(obj);
      break;
    case PRIMITIVE:
      printf("<PRIM>");
      break;
    case PROC:
      printf("<PROC>");
      break;
    default:
      printf("\nUnknown Object: %d\n", obj->type);
      break;
  }
}

Object *prim_cons(Object *args) {
  return cons(car(args), car(cdr(args)));
}

Object *prim_car(Object *args) {
  return car(car(args));
}

Object *prim_cdr(Object *args) {
  return cdr(car(args));
}

Object *primitive_eq_num(Object *a, Object *b) {
/*
  printf("Compare 2 nums\n");
  print_fixnum(a);
  print_fixnum(b);
*/
  int a_val = a->num.value;
  int b_val = b->num.value;

  if (a_val == b_val) {
    //printf("equal!\n");
    return s_t;
  } else {
    //printf("not equal!\n");
    return s_nil;
  }
}

Object *primitive_eq(Object *args) {
  if (is_fixnum(car(args)) &&
      is_fixnum(cadr(args)))
    return primitive_eq_num(car(args), cadr(args));
  else
    return s_nil;
}

void init() {

#ifdef GC_ENABLED
  current_mark = 1;
#endif

  for (int i = 0; i < MAX_ALLOC_SIZE; i++) {
    Object *obj = calloc(1, sizeof(struct Object));
    obj->id = i;
    obj->type = UNKNOWN;
    free_list[i] = obj;
  }
}

void run_gc_tests() {
  printf("\nv----------------------------------------v\n");
  printf("\nRunning GC tests.\n");

  //check_mem();

  // Build a lambda:
  // (define foo (lambda (a) a))
  // intern foo
  // cons symbol define, symbol foo, cons symbol lambda, cons symbol a, a
  Object *define = s_define; //intern_symbol("define");
  Object *foo = intern_symbol("foo");
  Object *lambda = s_lambda; //intern_symbol("lambda");
  Object *a = intern_symbol("a");

  printf("\n a @ %p\n", a);
  print(a);
  printf("\n");

  // (a)
  /* Object *args_car = make_cell(); */
  /* args_car->cell.car = a; */
  Object *args_car = cons(a, s_nil);
  print(args_car);
  printf("\n");

  // ((a) a)
  /* Object *lambda_car2 = make_cell(); */
  /* Object *lambda_cdr2 = lambda_car2; */
  /* lambda_car2->cell.car = args_car; */
  /* lambda_cdr2->cell.cdr = make_cell(); */
  /* lambda_cdr2->cell.cdr->cell.car = a; */
  //Object *args_car = cons(a, s_nil);

  // (lambda (a) a)
  /* Object *lambda_car1 = make_cell(); */
  /* Object *lambda_cdr1 = lambda_car1; */
  /* lambda_car1->cell.car = lambda; */
  /* /\* lambda_cdr1->cell.cdr = make_cell(); *\/ */
  /* /\* lambda_cdr1->cell.cdr->cell.car = lambda_car2; *\/ */
  /* lambda_car1->cell.cdr = make_cell(); */
  /* lambda_cdr1 = lambda_car1->cell.cdr; */
  /* lambda_cdr1->cell.car = args_car; */
  /* lambda_cdr1->cell.cdr = make_cell(); */
  /* lambda_cdr1 = lambda_cdr1->cell.cdr; */
  /* lambda_cdr1->cell.car = a; */
  Object *lambda_car1 = cons(lambda, cons(args_car, cons(a, s_nil)));
  print(lambda_car1);
  printf("\n");

  // (foo ...)
  /* Object *lambda_car3 = make_cell(); */
  /* Object *lambda_cdr3 = lambda_car3; */
  /* lambda_car3->cell.car = foo; */
  /* lambda_cdr3->cell.cdr = make_cell(); */
  /* lambda_cdr3->cell.cdr->cell.car = lambda_car1; */

  // (define foo (lambda (a) a))
  /* Object *define_car = make_cell(); */
  /* Object *define_cdr = define_car; */
  /* define_car->cell.car = define; */
  /* //define_cdr->cell.cdr = make_cell(); */
  /* //define_cdr->cell.cdr->cell.car = lambda_car3; */
  /* define_car->cell.cdr = make_cell(); */
  /* define_cdr = define_car->cell.cdr; */
  /* define_cdr->cell.car = foo; */
  /* define_cdr->cell.cdr = make_cell(); */
  /* define_cdr = define_cdr->cell.cdr; */
  /* define_cdr->cell.car = lambda_car1; */
  Object *define_car = cons(define, cons(foo, cons(lambda_car1, s_nil)));
  print(define_car);

  //Object *int_1 = make_fixnum(1);
  Object *foo_car1 = cons(foo, cons(make_fixnum(1), s_nil));
  print(foo_car1);
  printf("\n");

  Object *resultGC = NULL;

  printf("\nPass 1\n");
  for (int i = 0; i < 1; i++) {
    printf("\nPass 1 - %d\n", i);
    resultGC = eval(define_car, env);
    //printf("\n a @ %p\n", a);
    //print(a);
    //print(eval(a, env));
    //printf("---->\n");
    print(resultGC);
  }
  //return;

  // (foo 1)
  /* Object *foo_car1 = make_cell(); */
  /* foo_car1->cell.car = foo; */
  /* foo_car1->cell.cdr = make_cell(); */
  /* foo_car1->cell.cdr->cell.car = int_1; */

  printf("\n a @ %p\n", a);

  //print(lambda_car2);

  /* print(lambda_car3); */

  printf("---->\n");
  printf("---->\n");

  for (int i = 0; i < 20; i++) {
    //current_mark = 1000 + i;
    //gc();

    //current_mark = 1000;
    //gc();

    //current_mark = 10000;
    //gc();

    printf("\nPass 2 %d\n", i);
    /* resultGC = eval(define_car, env); */
    /* //resultGC = eval(foo_car1, env); */
    /* printf("---->\n"); */
    /* print(resultGC); */
    /* printf("\n"); */
    /* printf("\n"); */

    //current_mark = 100;
    printf("\n a @ %p\n", a);
    print(a);
    //print(eval(a, env));
    printf("\n");
    printf("\n");
    resultGC = eval(foo_car1, env);
    printf("\n a @ %p\n", a);
    print(a);
    //print(eval(a, env));
    printf("---->\n");
    print(resultGC);
    printf("\n");
    printf("\n");

  }
  /* resultGC = eval(define_car, env); */
  /* gc(); */
  /* printf("\nPass 3\n"); */
  /* resultGC = eval(define_car, env); */
}

void run_file_tests(char *fname) {
  printf("\n\nBEGIN FILE TESTS\n");

  FILE *fp = fopen(fname, "r");

  if (fp == NULL) {
    printf("File open failed: %d", errno);
    return;
  }

  Object *result = s_nil;

  while (result != NULL) {
    printf("\n----\nREAD\n");
    result = read_lisp(fp);
    //print(result);
    printf("\n----\nEVALUATE\n");
    print(result);
    result = eval(result, env);
    print(result);
    printf("\n");
    //gc();
  }

  fclose(fp);
  printf("END FILE TESTS\n");
}

int main(int argc, char* argv[]) {
  init();

  /* Make symbol nil (end of list). */
  s_nil = make_symbol("nil");

  /* Create symbol table.   */
  /*           'nil'   EOL  */
  /*             |      |   */
  /*             v      v   */
  symbols = cons(s_nil, s_nil);

  s_t = intern_symbol("t");
  s_lambda = intern_symbol("lambda");
  s_define = intern_symbol("define");
  s_quote = intern_symbol("quote");
  s_setq = intern_symbol("setq");
  s_if = intern_symbol("if");

  /* Create top level environment (list of lists).
   * Head is empty list and should never change,
   * so global references to env are stable
   * and changing the env does not require
   * returning a new head.
   */
  env = cons(cons(s_nil, s_nil), s_nil);
  extend_env(env, s_t, s_t);

  extend_env(env, intern_symbol("cons"), make_primitive(prim_cons));
  extend_env(env, intern_symbol("car"), make_primitive(prim_car));
  extend_env(env, intern_symbol("cdr"), make_primitive(prim_cdr));

  extend_env(env, intern_symbol("eq"), make_primitive(primitive_eq));

  extend_env(env, intern_symbol("+"), make_primitive(primitive_add));
  extend_env(env, intern_symbol("-"), make_primitive(primitive_sub));
  extend_env(env, intern_symbol("*"), make_primitive(primitive_mul));
  extend_env(env, intern_symbol("/"), make_primitive(primitive_div));

#ifdef CODE_TEST
  run_gc_tests();
#endif

#ifdef FILE_TEST
  run_file_tests("./test2.lsp");
#endif

#ifdef REPL
  printf("\nWelcome to JCM-LISP. Use ctrl-c to exit.\n");

  while (1) {
    Object *result = s_nil;

    printf("> ");
    result = read_lisp(stdin);
    result = eval(result, env);
    print(result);
    printf("\n");
  }
#endif

  return 0;
}
