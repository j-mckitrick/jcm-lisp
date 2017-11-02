/*
 * JCM-LISP
 *
 * Based on http://web.sonoma.edu/users/l/luvisi/sl3.c
 * and http://peter.michaux.ca/archives
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <assert.h>

#define MAX_BUFFER_SIZE 100

typedef enum
{
  NIL = 1,
  FIXNUM,
  STRING,
  SYMBOL,
  CELL,
  PRIMITIVE,
  PROC
} obj_type;

typedef struct object object;

struct Fixnum
{
  int value;
};

struct String
{
  char *text;
};

struct Symbol
{
  char *name;
};

struct Cell
{
  struct object *car;
  struct object *cdr;
};

struct Primitive
{
  object *(*fn)(object *args);
};

struct Proc
{
  struct object *vars;
  struct object *body;
  struct object *env;
};

struct object
{
  obj_type type;

  union
  {
    struct Fixnum num;
    struct String str;
    struct Symbol symbol;
    struct Cell cell;
    struct Primitive primitive;
    struct Proc proc;
  };
};

object *symbols;
object *quote_s;
object *define_s;
object *setq_s;
object *nil;
object *if_s;
object *t_s;

object *prim_add, *prim_sub, *prim_mul, *prim_div;

object *lambda_s;

int is_fixnum(object *obj)
{
  return (obj && obj->type == FIXNUM);
}

int is_string(object *obj)
{
  return (obj && obj->type == STRING);
}

int is_symbol(object *obj)
{
  return (obj && obj->type == SYMBOL);
}

int is_cell(object *obj)
{
  return (obj && obj->type == CELL);
}

int is_primitive(object *obj)
{
  return (obj && obj->type == PRIMITIVE);
}

int is_proc(object *obj)
{
  return (obj && obj->type == PROC);
}

object *car(object *obj)
{
  if (is_cell(obj))
    return obj->cell.car;
  else
    return nil;
}

object *cdr(object *obj)
{
  if (is_cell(obj))
    return obj->cell.cdr;
  else
    return nil;
}

#define caar(obj)    car(car(obj))
#define cadr(obj)    car(cdr(obj))

void setcar(object *obj, object *val)
{
  obj->cell.car = val;
}

void setcdr(object *obj, object *val)
{
  obj->cell.cdr = val;
}

object *new_object()
{
  object *obj = malloc(sizeof(object));
  return obj;
}

object *make_cell()
{
  object *obj = new_object();
  obj->type = CELL;
  obj->cell.car = nil;
  obj->cell.cdr = nil;
  return obj;
}

object *cons(object *car, object *cdr)
{
  object *obj = make_cell();
  obj->cell.car = car;
  obj->cell.cdr = cdr;
  return obj;
}

object *make_string(char *str)
{
  object *obj = new_object();
  obj->type = STRING;
  obj->str.text = strdup(str);
  return obj;
}

object *make_fixnum(int n)
{
  object *obj = new_object();
  obj->type = FIXNUM;
  obj->num.value = n;
  return obj;
}

object *make_symbol(char *name)
{
  object *obj = new_object();
  obj->type = SYMBOL;
  obj->symbol.name = strdup(name);
  return obj;
}

object *make_primitive(object *(*fn)(object *args))
{
  object *obj = new_object();
  obj->type = PRIMITIVE;
  obj->primitive.fn = fn;
  return obj;
}

object *make_proc(object *vars, object *body, object *env)
{
  object *obj = new_object();
  obj->type = PROC;
  obj->proc.vars = vars;
  obj->proc.body = body;
  obj->proc.env = env;
  return obj;
}

object *lookup_symbol(char *name)
{
  object *cell = symbols;
  object *sym;

  while (cell != nil)
  {
    sym = car(cell);

    if (is_symbol(sym) &&
        strcmp(sym->symbol.name, name) == 0)
    {
      return sym;
    }

    cell = cdr(cell);
  }

  return nil;
}

object *intern_symbol(char *name)
{
  object *sym = lookup_symbol(name);

  if (sym == nil)
  {
    sym = make_symbol(name);
    symbols = cons(sym, symbols);
  }

  return sym;
}

object *assoc(object *key, object *list)
{
  if (list != nil)
  {
    object *pair = car(list);

    if (car(pair) == key)
    {
      return pair;
    }

    return assoc(key, cdr(list));
  }

  return nil;
}

object *primitive_add(object *args)
{
  long total = 0;

  while (args != nil)
  {
    total += car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(total);
}

object *primitive_sub(object *args)
{
  long result = car(args)->num.value;

  args = cdr(args);
  while (args != nil)
  {
    result -= car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(result);
}

object *primitive_mul(object *args)
{
  long total = 1;

  while (args != nil)
  {
    total *= car(args)->num.value;
    args = cdr(args);
  }

  return make_fixnum(total);
}

object *primitive_div(object *args)
{
  long dividend = car(args)->num.value;
  long divisor = cadr(args)->num.value;
  long quotient = 0;

  if (divisor != 0)
  {
    quotient = dividend / divisor;
  }

  return make_fixnum(quotient);
}

int is_whitespace(char c)
{
  if (isspace(c))
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

int is_symbol_char(char c)
{
  return (isalnum(c) ||
          strchr("+-*/", c));
}

void skip_whitespace(FILE *in)
{
  char c;
  int done = 0;

  while (!done)
  {
    c = getc(in);
    if (c == '\n' && c == '\r')
    {
      done = 1;
    }

    if (!is_whitespace(c))
    {
      ungetc(c, in);
      done = 1;
    }
  }
}

object *read_string(FILE *in)
{
  char buffer[MAX_BUFFER_SIZE];
  int i = 0;
  char c;

  while ((c = getc(in)) != '"' &&
         i < MAX_BUFFER_SIZE - 1)
  {
    buffer[i++] = c;
  }

  buffer[i] = '\0';
  return make_string(buffer);
}

object *read_symbol(FILE *in)
{
  char buffer[MAX_BUFFER_SIZE];
  int i = 0;
  char c;

  while (!is_whitespace(c = getc(in)) &&
         is_symbol_char(c) &&
         i < MAX_BUFFER_SIZE - 1)
  {
    buffer[i++] = c;
  }

  buffer[i] = '\0';
  ungetc(c, in);

  object *obj = intern_symbol(buffer);

  return obj;
}

object *read_number(FILE *in)
{
  int number = 0;
  char c;

  while (isdigit(c = getc(in)))
  {
    number *= 10;
    number += (int)c - 48;
  }

  ungetc(c, in);

  return make_fixnum(number);
}

object *read_lisp(FILE *);
void print(object *);

object *read_list(FILE *in)
{
  char c;
  object *car, *cdr;

  car = cdr = make_cell();
  car->cell.car = read_lisp(in);

  while ((c = getc(in)) != ')')
  {
    if (c == '.')
    {
      getc(in);
      cdr->cell.cdr = read_lisp(in);
    }
    else if (!is_whitespace(c))
    {
      ungetc(c, in);

      cdr->cell.cdr = make_cell();
      cdr = cdr->cell.cdr;
      cdr->cell.car = read_lisp(in);
    }
  };

  return car;
}

// XXX Review these:
// strchr
// strdup
// strcmp
// strspn
// atoi
object *read_lisp(FILE *in)
{
  object *obj = nil;
  char c;

  skip_whitespace(in);
  c = getc(in);

  if (c == '\'')
  {
    obj = cons(quote_s, cons(read_lisp(in), nil));
  }
  else if (c == '(')
  {
    obj = read_list(in);
  }
  else if (c == '"')
  {
    obj = read_string(in);
  }
  else if (isdigit(c))
  {
    ungetc(c, in);
    obj = read_number(in);
  }
  else if (isalpha(c))
  {
    ungetc(c, in);
    obj = read_symbol(in);
  }
  //else if (c == '+')
  else if (strchr("+-*/", c))
  {
    ungetc(c, in);
    obj = read_symbol(in);
  }
  else if (c == ')')
  {
    ungetc(c, in);
  }
  else if (c == EOF)
  {
    exit(0);
  }

  return obj;
}

void error(char *msg)
{
  printf("Error %s\n", msg);
  exit(0);
}

object *eval_symbol(object *obj, object *env)
{
  if (obj == nil)
    return obj;

  object *tmp = assoc(obj, env);

  if (tmp != nil)
  {
    return cdr(tmp);
  }
  else
  {
    printf("Undefined symbol %s", obj->symbol.name);

    return nil;
  }
}

object *extend(object *env, object *var, object *val)
{
  return cons(cons(var, val), env);
}

object *extend_env(object* env, object *var, object *val)
{
  setcdr(env, extend(cdr(env), var, val));

  return val;
}

object *eval(object *obj, object *env);

object *eval_args(object *args, object *env)
{
  if (args != nil)
  {
    return cons(eval(car(args), env), eval_args(cdr(args), env));
  }

  return nil;
}

object *progn(object *forms, object *env)
{
  if (forms == nil)
    return nil;

  for (;;)
  {
    if (cdr(forms) == nil)
      return eval(car(forms), env);

    forms = cdr(forms);
  }

  return nil;
}

object *multiple_extend_env(object *env, object *vars, object *vals)
{
  if (vars == nil)
    return env;
  else
    return multiple_extend_env(extend(env, car(vars), car(vals)), cdr(vars), cdr(vals));
}

object *apply(object *proc, object *args, object *env)
{
  if (is_primitive(proc))
  {
    return (*proc->primitive.fn)(args);
  }

  if (is_proc(proc))
  {
    return progn(proc->proc.body, multiple_extend_env(env, proc->proc.vars, args));
  }

  error("Bad apply");

  return nil;
}

object *eval_list(object *obj, object *env)
{
  if (obj == nil)
    return obj;

  if (car(obj) == define_s)
  {
    object *cell = obj;
    //object *cell_define = car(cell); // should be symbol named define

    cell = cdr(cell);
    object *cell_symbol = car(cell);

    cell = cdr(cell);
    object *cell_value = car(cell);

    object *var = cell_symbol;
    object *val = eval(cell_value, env);
    extend_env(env, var, val);

    return var;
  }
  else if (car(obj) == setq_s)
  {
    object *cell = obj;
    //object *cell_setq = car(cell); // should be symbol named setq

    cell = cdr(cell);
    object *cell_symbol = car(cell);

    cell = cdr(cell);
    object *cell_value = car(cell);

    object *pair = assoc(cell_symbol, env);
    object *newval = eval(cell_value, env);
    setcdr(pair, newval);

    return newval;
  }
  else if (car(obj) == if_s)
  {
    object *cell = obj;
    //object *cell_if = car(cell);

    cell = cdr(cell);
    object *cell_condition = car(cell);

    cell = cdr(cell);
    object *cell_true_branch = car(cell);

    cell = cdr(cell);
    object *cell_false_branch = car(cell);

    if (eval(cell_condition, env) != nil)
    {
      return eval(cell_true_branch, env);
    }
    else
    {
      return eval(cell_false_branch, env);
    }
  }
  else if (car(obj) == quote_s)
  {
    return car(cdr(obj));
  }
  else if (car(obj) == lambda_s)
  {
    object *vars = car(cdr(obj));
    object *body = cdr(cdr(obj));
    return make_proc(vars, body, env);
  }

  object *proc = eval(car(obj), env);
  object *args = eval_args(cdr(obj), env);
  return apply(proc, args, env);
}

object *eval(object *obj, object *env)
{
  object *result = nil;

  if (obj == nil)
    return nil;

  switch (obj->type)
  {
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
      printf("\nUnknown object: %d\n", obj->type);
      break;
  }

  return result;
}

void print_string(object *obj)
{
  char *str = obj->str.text;
  int len = strlen(str);
  int i = 0;

  putchar('"');
  while (i < len)
  {
    putc(*str++, stdout);
    i++;
  }

  putchar('"');
}

void print_fixnum(object *obj)
{
  //printf("Fixnum\n");
  printf("%d", obj->num.value);
}

void print_cell(object *car)
{
  printf("(");
  object *obj = car;

  while (obj != nil)
  {
    if (obj->type == CELL)
    {
      print(obj->cell.car);
    }
    else
    {
      printf(". ");
      print(obj);
      break;
    }

    obj = obj->cell.cdr;

    if (obj != nil)
    {
      printf(" ");
    }
  }
  printf(")");
}

void print(object *obj)
{
  if (obj != nil)
  {
    switch (obj->type)
    {
      case CELL:
        print_cell(obj);
        break;
      case STRING:
        print_string(obj);
        break;
      case FIXNUM:
        print_fixnum(obj);
        break;
      case SYMBOL:
        printf("%s", obj->symbol.name);
        break;
      case PROC:
        printf("<PROC>");
        break;
      default:
        printf("\nUnknown object: %d\n", obj->type);
        break;
    }
  }
}

int main(int argc, char* argv[])
{
  nil = make_symbol("nil");
  symbols = cons(nil, nil);

  t_s = intern_symbol("t");
  quote_s = intern_symbol("quote");
  setq_s = intern_symbol("setq");
  define_s = intern_symbol("define");
  if_s = intern_symbol("if");
  lambda_s = intern_symbol("lambda");

  object *env = cons(cons(nil, nil), nil);
  extend_env(env, t_s, t_s);

  object *add_fn = make_primitive(primitive_add);
  object *sub_fn = make_primitive(primitive_sub);
  object *mul_fn = make_primitive(primitive_mul);
  object *div_fn = make_primitive(primitive_div);

  prim_add = intern_symbol("+");
  prim_sub = intern_symbol("-");
  prim_mul = intern_symbol("*");
  prim_div = intern_symbol("/");

  extend_env(env, prim_add, add_fn);
  extend_env(env, prim_sub, sub_fn);
  extend_env(env, prim_mul, mul_fn);
  extend_env(env, prim_div, div_fn);

  printf("Welcome to JCM-LISP. "
         "Use ctrl-c to exit.\n");

  while (1)
  {
    object *result = nil;

    printf("> ");
    result = read_lisp(stdin); /* Read should not need an env */
    result = eval(result, env);
    print(result);
    printf("\n");
  }

  return 0;
}
