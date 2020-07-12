#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <assert.h>

struct item {
	int prod;
	int dot;
	int la;
};

struct prod {
	int n;
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

/*
 * goal -> list
 * list -> list pair
 * list -> pair
 * pair -> ( pair )
 * pair -> ( )
 */
struct prod prods[] = {
	{ 0, 0, 1, { 1 } },
	{ 1, 1, 2, { 1, 2 } },
	{ 2, 1, 1, { 2 } },
	{ 3, 2, 3, { 3, 2, 4 } },
	{ 4, 2, 2, { 3, 4 } },
};
int nr_prods = 5;
int nr_syms = 5;
int eof = 5;
int epsilon = 5 + 1;

struct set *term;

struct set *first[5 + 2];

struct item items[1000];
int nr_items;

#define	MAX_CC 10000
struct set *ccs[MAX_CC];
int nr_ccs;

int act_tab[1000][7];
int goto_tab[1000][7];

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

	for (i = 0; i < nr_syms + 2; i++)
		first[i] = set_new(nr_syms + 2);
	for (i = 0; i < nr_syms; i++)
		if (term->s[i])
			set_add(first[i], i);
	set_add(first[epsilon], epsilon);
	set_add(first[eof], eof);

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
	for (i = 0; i < nr_syms; i++) {
		printf("first[%d] = ", i);
		for (j = 0; j < nr_syms + 2; j++)
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
			for (k = 0; k < nr_syms + 1; k++) {
				if (k != eof && !term->s[k])
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
		print_items(&items[i]);
#endif
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
				for (b = 0; b < nr_syms + 1; b++)
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
#if 0
				printf("cc%d = goto(cc%d, %d):\n",
				    nr_ccs,
				    c,
				    prods[items[i].prod].rhs[
				    items[i].dot]);
				print_cc(temp);
#endif
				ccs[nr_ccs++] = temp;
				assert(nr_ccs < MAX_CC);
			}
		}
	}
}

static void
add_act(int c, int s, int v)
{
	if (act_tab[c][s]) {
		if (act_tab[c][s] != v)
			printf("conflict act[%d][%d]: had %d want %d\n", c, s,
			    act_tab[c][s], v);
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
		act_tab[c][s] = v;
	}
}

static void
make_tables(void)
{
	struct prod *p;
	int c, g, i, sym;

	for (c = 0; c < nr_ccs; c++) {
		for (i = 0; i < nr_items; i++) {
			if (!ccs[c]->s[i])
				continue;
			p = &prods[items[i].prod];
			sym = p->rhs[items[i].dot];
			if (items[i].dot < p->nr_rhs && term->s[sym]) {
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
		for (i = 0; i < nr_syms; i++) {
			if (term->s[i])
				continue;
			g = find_cc(_goto(ccs[c], i));
			if (g >= 0)
				goto_tab[c][i] = g;
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
		if ((act_tab[s][w] & ACT_MASK) == REDUCE) {
			prod = &prods[act_tab[s][w] >> ACT_SHIFT];
			for (i = 0; i < prod->nr_rhs; i++) {
				pos--;
				pos--;
			}
			printf(" reduce %d\n", prod->lhs);
			s = stack[pos - 1];
			stack[pos++] = prod->lhs;
			stack[pos++] = goto_tab[s][prod->lhs];
		} else if ((act_tab[s][w] & ACT_MASK) == SHIFT) {
			printf(" shift %d\n", act_tab[s][w] >>
			    ACT_SHIFT);
			stack[pos++] = w;
			stack[pos++] = act_tab[s][w] >> ACT_SHIFT;
			w = yylex();
		} else if ((act_tab[s][w] & ACT_MASK) == ACCEPT) {
			printf(" accept\n");
			return (0);
		} else {
			printf(" fail\n");
			return (1);
		}
	}
}

/* (())()$ */
int _tok[] = { 3, 3, 4, 4, 3, 4, 5 };
int *tok = _tok;

int
yylex(void)
{
	return (*tok++);
}

int
main(void)
{
	term = set_new(nr_syms);
	set_add(term, 3);
	set_add(term, 4);

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
