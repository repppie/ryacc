#include <stdio.h>
#include <stdlib.h>
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
	int nr_rhs;
	int rhs[100];
};

struct set {
	int max;
	int s[];
};

enum {
	SHIFT = 1,
	REDUCE,
	ACCEPT,
};

#define	ACT_SHIFT 2
#define	ACT_MASK 3

int yylex(void);

struct prod prods[1000];

int nr_prods;
char *terms[1000];
char *nts[1000];
int nr_terms;
int nr_nts;
int eof;
int epsilon;

struct set *first[1000];

struct item items[100000];
int nr_items;

#define	MAX_CC 10000
struct set *ccs[MAX_CC];
int nr_ccs;

int *act_tab;
int *goto_tab;

enum {
	TOK_SECTION = 0x80,
	TOK_TOKEN,
	TOK_SYM,
};

static struct set *
set_new(int max)
{
	struct set *s;

	s = malloc(sizeof(struct set) + max * sizeof(int));
	s->max = max;
	memset(s->s, 0, sizeof(int) * max);
	return (s);
}

static struct set *
set_copy(struct set *_s)
{
	struct set *s;

	s = set_new(_s->max);
	memcpy(s->s, _s->s, sizeof(int) * _s->max);
	return (s);
}

static int
set_add(struct set *s, int v)
{
	int o;
	
	o = s->s[v];
	s->s[v] = 1;
	return (!o);
}

static void
set_remove(struct set *s, int v)
{
	s->s[v] = 0;
}

static int
set_union(struct set *a, struct set *b)
{
	int chg, i;

	chg = 0;
	for (i = 0; i < b->max; i++) {
		if (b->s[i]) {
			if (!a->s[i])
				chg |= 1;
			a->s[i] = 1;
		}
	}
	return (chg);
}

static void
make_first(void)
{
	struct set *rhs;
	int chg, i, p;

	for (i = 0; i < epsilon + nr_nts + 1; i++)
		first[i] = set_new(epsilon + nr_nts + 1);
	for (i = 0; i <= epsilon; i++)
		set_add(first[i], i);

	do {
		chg = 0;
		for (p = 0; p < nr_prods; p++) {
			if (prods[p].rhs[0] == epsilon)
				continue;
			rhs = set_copy(first[prods[p].rhs[0]]);
			set_remove(rhs, epsilon);
			for (i = 0; i < prods[p].nr_rhs - 1; i++) {
				if (!first[prods[p].rhs[i + 1]]->s[epsilon])
					break;
				set_union(rhs, first[prods[p].rhs[i + 1]]);
				set_remove(rhs, epsilon);
			}
			if (i == prods[p].nr_rhs)
				set_add(rhs, epsilon);
			chg |= set_union(first[prods[p].lhs], rhs);
			free(rhs);
		}
	} while (chg);

#if 0
	int j;
	for (i = 0; i < epsilon + nr_nts + 1; i++) {
		printf("first[%d] = ", i);
		for (j = 0; j < epsilon + nr_nts + 1; j++)
			if (first[i]->s[j])
				printf("%d ", j);
		printf("\n");
	}
#endif
}

static void
print_item(struct item *item)
{
	int i;

	printf("%d -> ", prods[item->prod].lhs);
	for (i = 0; i < prods[item->prod].nr_rhs; i++) {
		if (i == item->dot)
			printf(". ");
		printf("%d ", prods[item->prod].rhs[i]);
	}
	if (i == item->dot)
		printf(". ");
	printf("LA: %d\n", item->la);
}

static void
make_items(void)
{
	int i, j, k;

	for (i = 0; i < nr_prods; i++) {
		for (j = 0; j < prods[i].nr_rhs + 1; j++) {
			for (k = 0; k < epsilon; k++) {
				items[nr_items].prod = i;
				items[nr_items].dot = j;
				items[nr_items].la = k;
				nr_items++;
			}
		}
	}
#if 0
	for (i = 0; i < nr_items; i++)
		print_items(&items[i]);
#endif
	printf("%d items\n");
}

static int
find_item(int p, int dot, int la)
{
	int i;

	/* XXX store first_item in p in make_items? */
	for (i = 0; i < nr_items; i++)
		if (items[i].prod == p && items[i].dot == dot &&
		    items[i].la == la)
			return (i);
	errx(1, "couldn't find item p %d dot %d la %d\n", p, dot, la);
	return (-1);
}

static struct set *
closure(struct set *s)
{
	struct prod *p;
	int b, c, chg, i;
	
	do {
		chg = 0;
		for (i = 0; i < nr_items; i++) {
			if (!s->s[i])
				continue;
			/* XXX better way to find all prods? */
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
				if (items[i].dot >= p->nr_rhs - 1 ||
				    first[p->rhs[items[i].dot + 1]]->s[epsilon])
					chg |= set_add(s, find_item(c, 0,
					    items[i].la));
				for (b = 0; b < epsilon + nr_nts + 1; b++)
					if (first[p->rhs[items[i].dot+1]]->s[b])
						chg |= set_add(s, find_item(c,
						    0, b));
			}
		}
	} while (chg);

#if 0
	for (i = 0; i < nr_items; i++)
		if (s->s[i])
			print_item(&items[i]);
	printf("\n");
#endif

	return (s);
}

static void
print_cc(struct set *s)
{
	int i;

	for (i = 0; i < nr_items; i++)
		if (s->s[i])
			print_item(&items[i]);
	printf("\n");
}

static struct set *
_goto(struct set *s, int x)
{
	struct set *moved;
	int i;

	moved = set_new(nr_items);
	for (i = 0; i < nr_items; i++)
		if (s->s[i] && prods[items[i].prod].rhs[items[i].dot] == x &&
		    items[i].dot + 1 <= prods[items[i].prod].nr_rhs)
			set_add(moved, find_item(items[i].prod,
			    items[i].dot + 1, items[i].la));
	return (closure(moved));
}

static int
find_cc(struct set *s)
{
	int i;

	for (i = 0; i < nr_ccs; i++)
		if (!memcmp(s->s, ccs[i]->s, sizeof(int) * nr_items))
			return (i);
	return (-1);
}

static void
make_cc(void)
{
	struct set *cc, *cc0, *temp;
	int c, i, marked[MAX_CC];

	cc = set_new(MAX_CC);
	cc0 = set_new(nr_items);
	set_add(cc0, find_item(0, 0, eof));
	cc0 = closure(cc0);

#if 0
	printf("cc0 = ");
	print_cc(cc0);
#endif

	ccs[0] = cc0;
	nr_ccs = 1;
	memset(marked, 0, sizeof(int) * MAX_CC);

	for (c = 0; c < nr_ccs; c++) {
		if (marked[c])
			continue;
		marked[c] = 1;
		for (i = 0; i < nr_items; i++) {
			if (!ccs[c]->s[i] || items[i].dot >=
			    prods[items[i].prod].nr_rhs)
				continue;
			temp = _goto(ccs[c],
			    prods[items[i].prod].rhs[items[i].dot]);
			if (find_cc(temp) == -1) {
#if 1
				printf("cc%d = goto(cc%d, %d):\n",
				    nr_ccs,
				    c,
				    prods[items[i].prod].rhs[
				    items[i].dot]);
				//print_cc(temp);
#endif
				ccs[nr_ccs++] = temp;
				assert(nr_ccs < MAX_CC);
			}
		}
	}
	printf("%d ccs\n", nr_ccs);
}

static void
add_act(int c, int s, int v)
{
	if (act_tab[c * epsilon + s]) {
		if (act_tab[c * epsilon + s] != v)
			printf("conflict act[%d][%d]: had %d want %d\n", c, s,
			    act_tab[c * epsilon + s], v);
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
	int c, g, i, sym;

	act_tab = malloc(sizeof(int) * nr_items * epsilon);
	memset(act_tab, 0, sizeof(int) * nr_items * epsilon);
	goto_tab = malloc(sizeof(int) * nr_items * nr_nts);
	memset(goto_tab, 0, sizeof(int) * nr_items * nr_nts);

	for (c = 0; c < nr_ccs; c++) {
		for (i = 0; i < nr_items; i++) {
			if (!ccs[c]->s[i])
				continue;
			p = &prods[items[i].prod];
			sym = p->rhs[items[i].dot];
			if (items[i].dot < p->nr_rhs && sym < epsilon) {
				// XXX cache gotos?
				add_act(c, sym, SHIFT | (find_cc(_goto(ccs[c],
				    p->rhs[items[i].dot])) << ACT_SHIFT));
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
			g = find_cc(_goto(ccs[c], i));
			if (g >= 0)
				goto_tab[c * nr_nts + i - epsilon - 1] = g;
		}
	}
}

static int
parse(void)
{
	struct prod *prod;
	int i, pos, stack[1000], s, w;

	pos = 0;
	stack[pos++] = eof;
	stack[pos++] = 0;
	w = yylex();
	while (1) {
		s = stack[pos - 1];
		printf("s %d w %d stack", s, w);
		for (i = 0; i < pos; i++)
			printf(" %d", stack[i]);
		if ((act_tab[s * epsilon + w] & ACT_MASK) == REDUCE) {
			prod = &prods[act_tab[s * epsilon + w] >> ACT_SHIFT];
			for (i = 0; i < prod->nr_rhs; i++) {
				pos--;
				pos--;
			}
			printf(" reduce %d\n", prod->lhs);
			s = stack[pos - 1];
			stack[pos++] = prod->lhs;
			stack[pos++] = goto_tab[s * nr_nts + prod->lhs -
			    epsilon - 1];
		} else if ((act_tab[s * epsilon + w] & ACT_MASK) == SHIFT) {
			printf(" shift %d\n", act_tab[s * epsilon + w] >>
			    ACT_SHIFT);
			stack[pos++] = w;
			stack[pos++] = act_tab[s * epsilon + w] >> ACT_SHIFT;
			w = yylex();
		} else if ((act_tab[s * epsilon + w] & ACT_MASK) == ACCEPT) {
			printf(" accept\n");
			return (0);
		} else {
			printf(" fail\n");
			return (1);
		}
	}
}

/* (())()$ */
//int _tok[] = { 1, 1, 2, 2, 1, 2, 0 };
int _tok[] = { 7, 3, 5, 7, 1, 7, 6, 0 };
int *tok = _tok;

int
yylex(void)
{
	return (*tok++);
}

char _ytext[100];
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
	int c, s;

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
	}
	return (c);
}

static int
find_term(char *n)
{
	int i;

	for (i = 0; i < nr_terms; i++)
		if (!strcmp(terms[i], _ytext))
			return (i);
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
	epsilon = nr_terms + 1;

	while ((tok = _ylex()) == TOK_SYM) {
		nt = find_or_add_nt(_ytext);
		if (_ylex() != ':')
			errx(1, "expected ':' got %d at line %d", tok, line);
		prods[nr_prods].lhs = nt + epsilon + 1;
		while ((tok = _ylex()) == TOK_SYM) {
			if ((i = find_term(_ytext) + 1) == 0)
				i = find_or_add_nt(_ytext) + epsilon + 1;
			if (i > epsilon && _ytext[0] == '\'')
				errx(1, "didn't declare token %s", _ytext);
			prods[nr_prods].rhs[prods[nr_prods].nr_rhs] = i;
			prods[nr_prods].nr_rhs++;
		}
		if (tok != ';')
			errx(1, "expected ';' got %d at line %d", tok, line);
		nr_prods++;
	}
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
	if (parse())
		printf("syntax error\n");
	else
		printf("ok\n");

	return (0);
}
