#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <err.h>
#include <assert.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

struct item {
	int prod;
	int dot;
	int la;
};

struct prod {
	int lhs;
	int first_item;
	int nr_rhs;
	int rhs[100];
	char *act;
};

struct sset {
	int max;
	int n;
	int *l;
	int *v;
};

enum {
	SHIFT = 1,
	REDUCE,
	ACCEPT,
};

#define	ACT_SHIFT 2
#define	ACT_MASK 3

struct prod prods[1000];

int nr_prods;
char *terms[1000];
char *nts[1000];
struct sset *char_literals;
int nr_terms;
int nr_nts;
int eof;
int epsilon;

struct sset *first[1000];

struct item items[300000];
int nr_items;

#define	MAX_CC 10000
struct sset *ccs[MAX_CC];
int gotos[MAX_CC][1000];
int nr_ccs;

int *act_tab;
int *goto_tab;

int out_fd;
char *verbatim;

enum {
	TOK_SECTION = 0x80,
	TOK_TOKEN,
	TOK_SYM,
	TOK_ACT,
};

static struct sset *
sset_new(int max)
{
	struct sset *s;

	s = malloc(sizeof(struct sset));
	s->l = malloc(sizeof(int) * max);
	s->v = malloc(sizeof(int) * max);
	s->max = max;
	s->n = 0;
	return (s);
}

static void
sset_free(struct sset *s)
{
	free(s->l);
	free(s->v);
	free(s);
}

struct sset *
sset_copy(struct sset *s)
{
	int i;
	struct sset *new;

	new = sset_new(s->max);
	memcpy(new->l, s->l, s->n * sizeof(int));
	for (i = 0; i < s->n; i++)
		new->v[s->l[i]] = s->v[s->l[i]];
	new->n = s->n;
	return (new);
}

static int
sset_has(struct sset *s, int v)
{
	return (s->v[v] >= 0 && s->v[v] < s->n && s->l[s->v[v]] == v);
}

static int
sset_add(struct sset *s, int v)
{
	if (v >= s->max || sset_has(s, v))
		return (0);
	s->v[v] = s->n;
	s->l[s->n++] = v;
	return (1);
}

static void
sset_remove(struct sset *s, int v)
{
	int t;
	
	if (!sset_has(s, v))
		return;
	t = s->l[s->n - 1];
	s->l[s->v[v]] = t;
	s->l[t] = s->v[v];
	s->n--;
}

static int
sset_union(struct sset *a, struct sset *b)
{
	int chg, i;

	chg = 0;
	for (i = 0; i < b->n; i++)
		chg |= sset_add(a, b->l[i]);
	return (chg);
}

static void
make_first(void)
{
	struct sset *rhs;
	int chg, i, p;

	for (i = 0; i < epsilon + nr_nts + 1; i++)
		if ((i < 256 && sset_has(char_literals, i)) || i >= 256)
			first[i] = sset_new(epsilon + nr_nts + 1);
	for (i = 0; i <= epsilon; i++)
		if (i == 0 || (i < 256 && sset_has(char_literals, i)) || i >=
		    256)
			sset_add(first[i], i);

	do {
		chg = 0;
		for (p = 0; p < nr_prods; p++) {
			if (prods[p].rhs[0] == epsilon)
				continue;
			rhs = sset_copy(first[prods[p].rhs[0]]);
			sset_remove(rhs, epsilon);
			for (i = 0; i < prods[p].nr_rhs - 1; i++) {
				if (!sset_has(first[prods[p].rhs[i + 1]],
				    epsilon))
					break;
				sset_union(rhs, first[prods[p].rhs[i + 1]]);
				sset_remove(rhs, epsilon);
			}
			if (i == prods[p].nr_rhs)
				sset_add(rhs, epsilon);
			chg |= sset_union(first[prods[p].lhs], rhs);
			sset_free(rhs);
		}
	} while (chg);

#if 0
	int j;
	for (i = 0; i < epsilon + nr_nts + 1; i++) {
		if (!first[i])
			continue;
		printf("first[%d] = ", i);
		for (j = 0; j < epsilon + nr_nts + 1; j++)
			if (sset_has(first[i], j))
				printf("%d ", j);
		printf("\n");
	}
#endif
}

static void
print_item(struct item *item)
{
	int i, r;

	printf("%s -> ", nts[prods[item->prod].lhs - epsilon - 1]);
	for (i = 0; i < prods[item->prod].nr_rhs; i++) {
		r =  prods[item->prod].rhs[i];
		if (i == item->dot)
			printf(". ");
		if (r < 256)
			printf("'%c' ", r);
		else if (r < epsilon)
			printf("%s ", terms[r - 256]);
		else
			printf("%s ", nts[r - epsilon - 1]);
	}
	if (i == item->dot)
		printf(". ");
	if (item->la < 256)
		printf("LA: '%c'\n", item->la);
	else
		printf("LA: '%s'\n", terms[item->la - 256]);
}

static void
make_items(void)
{
	int i, j, k;

	for (i = 0; i < nr_prods; i++) {
		prods[i].first_item = nr_items;
		for (j = 0; j < prods[i].nr_rhs + 1; j++) {
			for (k = 0; k < epsilon; k++) {
				if (k < 256 && !sset_has(char_literals, k))
					continue;
				items[nr_items].prod = i;
				items[nr_items].dot = j;
				items[nr_items].la = k;
				nr_items++;
			}
		}
	}
#if 0
	for (i = 0; i < nr_items; i++)
		print_item(&items[i]);
#endif
	printf("%d items\n", nr_items);
}

static int
find_item(int p, int dot, int la)
{
	int i;

	for (i = prods[p].first_item; items[i].prod == p; i++)
		if (items[i].dot == dot && items[i].la == la)
			return (i);
	errx(1, "couldn't find item p %d dot %d la %d\n", p, dot, la);
	return (-1);
}

static struct sset *
closure(struct sset *s)
{
	struct prod *p;
	struct sset *fst;
	int b, c, chg, i, k;
	
	do {
		chg = 0;
		for (k = 0; k < s->n; k++) {
			i = s->l[k];
			/*
			 * XXX better way to find all prods?
			 * Maybe have a set of productions for each item set.
			 */
			for (c = 0; c < nr_prods; c++) {
				p = &prods[items[i].prod];
				if (p->rhs[items[i].dot] != prods[c].lhs)
					continue;
				if (items[i].dot >= p->nr_rhs)
					continue;
#if 0
				printf("p %d la %d dot %d (%d) c %d\n",
				    items[i].prod, items[i].la, 
				    items[i].dot,
				    p->rhs[items[i].dot],
				    c);
#endif
				/*
				 * Let p -> A dot C B.
				 * Would C complete the production?
				 * Yes if either B = epsilon
				 * or epsilon \in first(C).
				 */
				if (items[i].dot >= p->nr_rhs - 1 ||
				    sset_has(first[p->rhs[items[i].dot + 1]],
				    epsilon))
					chg |= sset_add(s, find_item(c, 0,
					    items[i].la));
				fst = first[p->rhs[items[i].dot + 1]];
				for (b = 0; b < fst->n; b++)
					sset_add(s, find_item(c, 0, fst->l[b]));
			}
		}
	} while (chg);

#if 0
	for (i = 0; i < s->n; i++)
		print_item(&items[s->l[i]]);
	printf("\n");
#endif

	return (s);
}

static void
print_cc(struct sset *s)
{
	struct sset *ss;
	int i;

	ss = sset_new(nr_prods);
	for (i = 0; i < s->n; i++)
		if (sset_add(ss, items[s->l[i]].prod))
			print_item(&items[s->l[i]]);
	sset_free(ss);
	printf("\n");
}

static struct sset *
_goto(struct sset *s, int x)
{
	struct sset *moved;
	int i, k;

	moved = sset_new(nr_items);
	for (k = 0; k < s->n; k++) {
		i = s->l[k];	
		if (prods[items[i].prod].rhs[items[i].dot] == x &&
		    items[i].dot + 1 <= prods[items[i].prod].nr_rhs) {
			int it = find_item(items[i].prod, items[i].dot + 1,
			    items[i].la);
			sset_add(moved, it);
		}
	}
	return (closure(moved));
}

static int
find_cc(struct sset *s)
{
	int i, j, same;

	for (i = 0; i < nr_ccs; i++) {
		if (s->n != ccs[i]->n)
			continue;
		same = 1;
		for (j = 0; j < s->n; j++) {
			if (!sset_has(ccs[i], s->l[j])) {
				same = 0;
				break;
			}
		}
		if (same)
			return (i);
			
	}
	return (-1);
}

static void
make_cc(void)
{
	struct sset *cc0, *temp;
	int c, f, i, j, k, w;

	cc0 = sset_new(nr_items);
	sset_add(cc0, find_item(0, 0, eof));
	cc0 = closure(cc0);

#if 0
	printf("cc0 = ");
	print_cc(cc0);
#endif

	ccs[0] = cc0;
	nr_ccs = 1;

	for (i = 0; i < MAX_CC; i++)
		for (j = 0; j < 1000; j++)
			gotos[i][j] = -1;

	for (c = 0; c < nr_ccs; c++) {
		for (k = 0; k < ccs[c]->n; k++) {
			i = ccs[c]->l[k];
			if (items[i].dot >= prods[items[i].prod].nr_rhs)
				continue;
			w = prods[items[i].prod].rhs[items[i].dot];
			if (gotos[c][w] >= 0)
				continue;
			temp = _goto(ccs[c], w);
			if ((f = find_cc(temp)) == -1) {
#if 0
				printf("cc%d = goto(cc%d, %d):\n",
				    nr_ccs,
				    c,
				    prods[items[i].prod].rhs[
				    items[i].dot]);
				//print_cc(temp);
				int j, sum, s2;
				sum = s2 = 0;
				for (j = 0; j < temp->n; j++) {
					if (items[temp->l[j]].dot != 0)
						sum++;
				}
				printf("nr: %d/%d\n", sum, temp->n);
				fflush(stdout);
#endif
				gotos[c][w] = nr_ccs;
				ccs[nr_ccs++] = temp;
				assert(nr_ccs < MAX_CC);
			} else {
				sset_free(temp);
				gotos[c][w] = f;
			}
		}
	}
	printf("%d ccs\n", nr_ccs);
}

void
print_conflict(int cc, int s, int v)
{
	printf("conflict act[%d][%d]: had %d want %d on token ", cc, s,
	    act_tab[cc * epsilon + s], v);
	if (s < 256)
		printf("'%c'\n", s);
	else
		printf("%s\n", terms[s - 257]);
	print_cc(ccs[cc]);
}

static void
add_act(int c, int s, int v)
{
	if (act_tab[c * epsilon + s]) {
		if (act_tab[c * epsilon + s] != v)
			print_conflict(c, s, v);
	} else {
#if 0
		printf("act[%d, %d] = ", c, s);
		if ((v & ACT_MASK) == SHIFT)
			printf("shift %d\n", v >> ACT_SHIFT);
		else if ((v & ACT_MASK) == REDUCE)
			printf("reduce %d\n", v >> ACT_SHIFT);
		else
			printf("accept\n");
#endif
		act_tab[c * epsilon + s] = v;
	}
}

static void
make_tables(void)
{
	struct prod *p;
	int c, g, i, k, sym;

	act_tab = malloc(sizeof(int) * nr_items * epsilon);
	memset(act_tab, 0, sizeof(int) * nr_ccs * epsilon);
	goto_tab = malloc(sizeof(int) * nr_ccs * nr_nts);
	memset(goto_tab, 0, sizeof(int) * nr_items * nr_nts);

	for (c = 0; c < nr_ccs; c++) {
		for (k = 0; k < ccs[c]->n; k++) {
			i = ccs[c]->l[k];
			p = &prods[items[i].prod];
			sym = p->rhs[items[i].dot];
			if (items[i].dot < p->nr_rhs && sym < epsilon) {
				add_act(c, sym, SHIFT |
				    gotos[c][p->rhs[items[i].dot]] <<
				    ACT_SHIFT);
			} else if (items[i].dot >= p->nr_rhs) {
				if (items[i].prod == 0) {
					add_act(c, items[i].la, ACCEPT);
					continue;
				}
				add_act(c, items[i].la, REDUCE |
				    (items[i].prod << ACT_SHIFT));
			}
		}
		for (i = epsilon + 1; i < epsilon + nr_nts + 1; i++) {
			g = gotos[c][i];
			if (g >= 0)
				goto_tab[c * nr_nts + i - epsilon - 1] = g;
		}
	}
}

static void
emit(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (vdprintf(out_fd, fmt, args) < 0)
		err(1, "vdprintf");
	va_end(args);
}

static void
emit_act(int i)
{
	char *e, *p;
	int n;

	emit("\t\t\t\t{\n");
	emit("\t\t\t\t");
	for (p = prods[i].act; *p; p++) {
		if (*p == '\n')
			emit("\t\t\t\t");
		else if (*p == '$') {
			if (p[1] == '$') {
				emit("yyval");
				p++;
				continue;
			} else {
				n = strtol(p + 1, &e, 10);
				if (n > prods[i].nr_rhs)
					errx(1, "$%d refereces beyond end of "
					    "rule for %s", n, nts[prods[i].lhs -
					    epsilon - 1]);
				if (n == 0)
					n = 1;
				emit("stack[pos-%d]", n * 2);
				p += e - p;
			}
		}
		emit("%c", *p);
	}
	emit("\t\t\t\t}\n");
}

static void
emit_parser(void) {
	int i, j, n;

	if ((out_fd = open("y.tab.c", O_CREAT | O_WRONLY)) < 0)
		err(1, "open y.tab.c");
	if (ftruncate(out_fd, 0) != 0)
		err(1, "ftruncate");
	
	emit("static int act_tab[%d][%d] = { ", nr_ccs, epsilon);
	n = 0;
	for (i = 0; i < nr_ccs; i++) {
		for (j = 0; j < epsilon; j++) {
			if ((++n % 15) == 0)
				emit("\n");
			emit("%d,", act_tab[i * epsilon + j]);
		}
	}
	emit("};\n");
	emit("static int goto_tab[%d][%d] = { ", nr_ccs, nr_nts);
	for (i = 0; i < nr_ccs; i++) {
		for (j = 0; j < nr_nts; j++) {
			if ((++n % 15) == 0)
				emit("\n");
			emit("%d,", goto_tab[i * nr_nts + j]);
		}
	}
	emit("};\n");
	emit("static int prods[%d] = { ", nr_prods * 2);
	for (i = 0; i < nr_prods; i++) {
			if ((++n % 15) == 0)
				emit("\n");
			emit("%d,", prods[i].nr_rhs);
			emit("%d,", prods[i].lhs - epsilon - 1);
	}
	emit("};\n");

	emit("#include <stdio.h>\n");
	emit("extern int yylex();\n");
	emit("int yylval;\n");
	emit("int yyval;\n");
	emit("static int yyparse(void) {\n");
	emit("\tint i, pos, prod, stack[1000], s, w;\n");
	emit("\tpos = 0; stack[pos++] = %d; stack[pos++] = 0;\n", eof);
	emit("\tw = yylex();\n");
	emit("\twhile (1) {\n");
	emit("\t\ts = stack[pos - 1];\n");
	emit("\t\tprintf(\"s %%d w %%d stack\", s, w);\n");
	emit("\t\tfor (i = 0; i < pos; i ++)\n");
	emit("\t\t\tprintf(\" %%d\", stack[i]);\n");
	emit("\t\tif ((act_tab[s][w] & %d) == %d) {\n", ACT_MASK, REDUCE);
	emit("\t\t\tprintf(\" reduce %%d\\n\", prods[prod * 2 + 1]);\n");
	emit("\t\t\tprod = act_tab[s][w] >> %d;\n", ACT_SHIFT);
	emit("\t\t\tyyval = stack[pos - 2 * prods[prod * 2]];\n");
	emit("\t\t\tswitch (prod) {\n");
	for (i = 0; i < nr_prods; i++) {
		if (prods[i].act == NULL)
			continue;
		emit("\t\t\tcase %d:\n", i);
		emit_act(i);
		emit("\n\t\t\t\tbreak;\n");
	}
	emit("\t\t\t}\n");
	emit("\t\t\tpos -=  prods[prod * 2] * 2;\n");
	emit("\t\t\ts = stack[pos - 1];\n");
	emit("\t\t\tstack[pos++] = yyval;\n");
	emit("\t\t\tstack[pos++] = goto_tab[s][prods[prod*2+1]];\n");
	emit("\t\t} else if ((act_tab[s][w] & %d) == %d) {\n", ACT_MASK, SHIFT);
	emit("\t\t\tprintf(\" shift %%d\\n\", act_tab[s][w] >> %d);\n",
	    ACT_SHIFT);
	emit("\t\t\tstack[pos++] = yylval;\n");
	emit("\t\t\tstack[pos++] = act_tab[s][w] >> %d;\n", ACT_SHIFT);
	emit("\t\t\tw = yylex();\n");
	emit("\t\t} else if ((act_tab[s][w] & %d) == %d) {\n", ACT_MASK,
	    ACCEPT);
	emit("\t\t\tprintf(\" accept\\n\");\n");
	if (prods[0].act != NULL)
		emit_act(0);
	emit("\t\t\treturn (0);\n");
	emit("\t\t} else {\n");
	emit("\t\t\tprintf(\" fail\\n\");\n");
	emit("\t\t\treturn (1);\n");
	emit("\t\t}\n");
	emit("\t}\n");
	emit("}\n");

	if (verbatim)
		for (; *verbatim != '\0'; verbatim++)
			emit("%c", *verbatim);
}

char _ytext[100];
char *_yptr;
char *m;
int mpos;
int line = 1;

static int
next_char(void)
{
	int c;

	c = m[mpos++];
	if (c == '\n')
		line++;
	return (c);
}

static int
_ylex(void)
{
	int c, nest, s;

	c = next_char();
	while (isspace(c))
		c = next_char();

	memset(_ytext, 0, 100);
	if (c == '%') {
		if ((c = m[mpos]) == '%') {
			mpos++;
			return (TOK_SECTION);
		}
		for (s = mpos; !isspace(m[mpos]); mpos++);
		if (!strncmp(m + s, "token", mpos - s))
			return (TOK_TOKEN);
	} else if (isalpha(c) || c == '_') {
		for (s = mpos; isalpha(m[mpos]) || m[mpos] == '_'; mpos++);
		memcpy(_ytext, m + s - 1, mpos - s + 1);
		return (TOK_SYM);
	} else if (c == '\'') {
		_ytext[0] = '\'';
		_ytext[1] = m[mpos];
		_ytext[2] = '\'';
		mpos += 2;
		return (TOK_SYM);
	} else if (c == '{') {
		nest = 0;
		s = mpos++;
		while (1) {
			if (m[mpos] == '{')
				nest++;
			else if (m[mpos] == '}' && nest-- == 0)
				break;
			mpos++;
		}
		m[mpos++] = '\0';
		_yptr = m + s;
		return (TOK_ACT);
	}
	return (c);
}

static int
find_term(char *n)
{
	int i;

	if (*n == '\'') {
		sset_add(char_literals, *++n);
		return (*n);
	}

	for (i = 0; i < nr_terms; i++)
		if (!strcmp(terms[i], _ytext))
			return (i + 256);
	return (-1);
}

static int
find_or_add_nt(char *n)
{
	int i, found;

	found = 0;
	for (i = 0; i < nr_nts; i++) {
		if (!strcmp(nts[i], n)) {
			found = 1;
			break;
		}
	}		
	if (!found)
		nts[nr_nts++] = strdup(_ytext);
	return (i);
}

static void
parse_y(char *path)
{
	struct stat sb;
	int fd, i, nt, tok;

	if ((fd = open(path, O_RDONLY)) < 0)
		err(1, "open");
	if (fstat(fd, &sb) != 0)
		err(1, "fstat");
	if ((m = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd,
	    0)) == MAP_FAILED)
		err(1, "mmap");

	char_literals = sset_new(256);
	sset_add(char_literals, 0);

	tok = _ylex();
	while (tok == TOK_TOKEN) {
		while ((tok = _ylex()) == TOK_SYM) {
			if (find_term(_ytext) != -1)
				errx(1, "duplicate token %s at line %d\n",
				    _ytext, line);
			terms[nr_terms++] = strdup(_ytext);
		}
	}
	if (tok != TOK_SECTION)
		errx(1, "expected %%%% got %d at line %d", tok, line);
	printf("%d terms\n", nr_terms);
	epsilon = 256 + nr_terms;

	while ((tok = _ylex()) == TOK_SYM) {
		nt = find_or_add_nt(_ytext);
		if (_ylex() != ':')
			errx(1, "expected ':' got %d at line %d", tok, line);
		do {
			prods[nr_prods].lhs = nt + epsilon + 1;
			while ((tok = _ylex()) == TOK_SYM) {
				if ((i = find_term(_ytext)) == -1)
					i = find_or_add_nt(_ytext) + epsilon +
					    1;
				if (i > epsilon && _ytext[0] == '\'')
					errx(1, "didn't declare token %s",
					    _ytext);
				prods[nr_prods].rhs[prods[nr_prods].nr_rhs] = i;
				prods[nr_prods].nr_rhs++;
			}
			if (tok == TOK_ACT) {
				prods[nr_prods].act = _yptr;
				tok = _ylex();
			}
			nr_prods++;
		} while (tok == '|');
		if (tok != ';')
			errx(1, "expected ';' got 0x%x at line %d", tok, line);
	}
	if (tok == TOK_SECTION)
		verbatim = &m[mpos];

	printf("%d nts\n", nr_nts);
	printf("%d productions\n", nr_prods);
}

int
main(int argc, char **argv)
{
	if (argc < 2)
		errx(1, "Usage: %s <file>", argv[0]);
	parse_y(argv[1]);

	make_first();
	make_items();
	make_cc();
	make_tables();

	emit_parser();

	return (0);
}
