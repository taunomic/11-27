//#define READLINE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include <iostream>
#include <ostream>
#include "misc.h"
#include <utility>
#include "object.h"
bidict& gdict = dict;
#include "prover.h"

using namespace std;
extern ostream& dout;

int logequalTo, lognotEqualTo, rdffirst, rdfrest, A, rdfsResource, rdfList, Dot, GND;

namespace prover {

term* terms = 0;
termset* termsets = 0;
rule* rules = 0;
proof* proofs = 0;
uint nterms, ntermsets, nrules, nproofs;

void printps(term* p, subst* s); // print a term with a subst

void initmem() {
	if (terms) delete[] terms;
	if (termsets) delete[] terms;
	if (rules) delete[] terms;
	if (proofs) delete[] terms;
	terms = new term[max_terms];
	termsets = new termset[max_termsets];
	rules = new rule[max_rules];
	proofs = new proof[max_proofs];
	nterms = ntermsets = nrules = nproofs = 0;
}

term* evaluate(term* p, subst& s) {
	if (p->p < 0) {
		auto it = s.find(p->p);
		return it == s.end() ? 0 : evaluate(s[p->p], s);
	}
	if (!p->s && !p->o)
		return p;
	term *a = evaluate(p->s, s), *r = &terms[nterms++];
	r->p = p->p;
	if (a)
		r->s = a;
	else {
		r->s = &terms[nterms++];
		r->s->p = p->s->p;
		r->s->s = r->s->o = 0;
	}
	a = evaluate(p->o, s);
	if (a)
		r->o = a;
	else {
		r->o = &terms[nterms++];
		r->o->p = p->o->p;
		r->o->s = r->o->o = 0;
	}
	return r;
}

void printps(term& p, subst& s) {
	printterm(p);
	dout<<" [ ";
	prints(s);
	dout<<" ] ";
}

bool unify(term* s, subst& ssub, term* d, subst& dsub, bool f) {
	term* v;
	if (!s && !d) return true;
	if (s->p < 0) 
		return (v = evaluate(s, ssub)) ? unify(v, ssub, d, dsub, f) : true;
	if (d->p < 0) {
		if ((v = evaluate(d, dsub)))
			return unify(s, ssub, v, dsub, f);
		else {
			if (f)
				dsub[d->p] = evaluate(s, ssub);
			return true;
		}
	}
	if (!(s->p == d->p && !s->s == !d->s && !s->o == !d->o))
		return false;
	return unify(s->s, ssub, d->s, dsub, f) && unify(s->o, ssub, d->o, dsub, f);
}

bool euler_path(proof* p) {
	proof* ep = p;
	while ((ep = ep->prev))
		if (ep->rul == p->rul &&
			unify(ep->rul->p, ep->s, p->rul->p, p->s, false))
			break;
	if (ep) {
		TRACE(dout<<"Euler path detected\n");
		return true;
	}
	return false;
}

int builtin(term& t, proof& p, session*) {
	if (t.p == GND) return 1;
	term* t0 = evaluate(t.s, p.s);
	term* t1 = evaluate(t.o, p.s);
	if (t.p == logequalTo)
		return t0 && t1 && t.p == t1->p ? 1 : 0;
	if (t.p == lognotEqualTo)
		return t0 && t1 && t0->p != t1->p ? 1 : 0;
	if (t.p == rdffirst && t0 && t0->p == Dot && (t0->s || t0->o))
			return unify(t0->s, p.s, t.o, p.s, true) ? 1 : 0;
	if (t.p == rdfrest && t0 && t0->p == Dot && (t0->s || t0->o))
			return unify(t0->o, p.s, t.o, p.s, true) ? 1 : 0;
	if (t.p == A) {
		if (!t1) 
			return -1;
		if ((t1->p == rdfList && t0 && t0->p == Dot) || t1->p == rdfsResource)
			return 1;
	}
	return -1;
}

void prove(session* ss) {
	termset& goal = ss->goal;
	ruleset& cases = ss->rkb;
	queue qu;
	rule* rg = &rules[nrules++];
	proof* p = &proofs[nproofs++];
	rg->p = 0;
	rg->body = goal;
	p->rul = rg;
	p->last = rg->body.begin();
	p->prev = 0;
	qu.push_back(p);
	TRACE(dout<<"\nprove() called with facts:";
		printrs(cases);
		dout<<"\nand query:";
		printl(goal);
		dout<<endl);
	do {
		p = qu.back();
		qu.pop_back();
		if (p->last != p->rul->body.end()) {
			term* t = *p->last;
			TRACE(dout<<"Tracking back from ";
				printterm(*t);
				dout<<endl);
			int b = builtin(*t, *p, ss);
			if (b == 1) {
				proof* r = &proofs[nproofs++];
				rule* rl = &rules[nrules++];
				*r = *p;
				rl->p = evaluate(t, p->s);
				r->g = ground(p->g);
				r->g.emplace_back(rl, subst());
				++r->last;
				qu.push_back(r);
		            	continue;
		        }
		    	else if (!b) 
				continue;

			auto it = cases.find(t->p);
			if (it == cases.end())
				continue;
//			for (auto x : cases) {
//			if (x.first >= 0 && x.first != t->p)
//				continue;
			list<rule*>& rs = it->second;
			for (rule* rl : rs) {
				subst s;
				TRACE(dout<<"\tTrying to unify ";
					printps(*t, p->s);
					dout<<" and ";
					printps(*rl->p, s);
					dout<<"... ");
				if (unify(t, p->s, rl->p, s, true)) {
					TRACE(dout<<"\tunification succeeded";
						if (s.size()) {
							dout<<" with new substitution: ";
							prints(s);
						};
						dout<<endl);
					if (euler_path(p))
						continue;
					proof* r = &proofs[nproofs++];
					r->rul = rl;
					r->last = r->rul->body.begin();
					r->prev = p;
					r->s = s;
					r->g = ground(p->g);
					if (rl->body.empty())
						r->g.emplace_back(rl, subst());
					qu.push_front(r);
				} else {
					TRACE(dout<<"\tunification failed\n");
				}
			}
		}//}
		else if (!p->prev) {
			for (auto r = p->rul->body.begin(); r != p->rul->body.end(); ++r) {
				term* t = evaluate(*r, p->s);
				ss->e[t->p].emplace_back(t, p->g);
			}
			TRACE(dout<<"no prev frame. queue:\n");
//			TRACE(printq(qu));
		} else {
			ground g = p->g;
			proof* r = &proofs[nproofs++];
			if (!p->rul->body.empty())
				g.emplace_back(p->rul, p->s);
			*r = *p->prev;
			r->g = g;
			r->s = p->prev->s;
			unify(p->rul->p, p->s, *r->last, r->s, true);
			++r->last;
			qu.push_back(r);
			TRACE(dout<<"finished a frame. queue size:" << qu.size() << endl);
//			TRACE(printq(qu));
			continue;
		}
	} while (!qu.empty());
	dout<<"\nEvidence:";
	dout<<"========="<<endl;
	printe(ss->e);
}

bool equals(term* x, term* y) {
	return (!x == !y) && x && equals(x->s, y->s) && equals(x->o, y->o);
}

void printterm(const term& p) {
	if (p.s)
		printterm(*p.s);
	dout<<' '<<dstr(p.p)<<' ';
	if (p.o)
		printterm(*p.o);
}

void printp(proof* p) {
	if (!p)
		return;
	dout<<"\trule:\t";
	printr(*p->rul);
	dout<<"\n\tremaining:\t";
//	printl(p->last);
	if (p->prev)
		dout<<"\n\tprev:\t"<<p->prev<<"\n\tsubst:\t";
	else
		dout<<"\n\tprev:\t(null)\n\tsubst:\t";
	prints(p->s);
	dout<<"\n\tground:\t";
	printg(p->g);
	dout<<"\n";
}

void printrs(const ruleset& rs) {
	for (auto x : rs)
		for (auto y : x.second) {
			printr(*y);
			dout << endl;
		}
}

void printq(const queue& q) {
	for (auto x : q) {
		printp(x);
		dout << endl;
	}
}

void prints(const subst& s) {
//	dout << '[';
	for (auto x : s) {
		dout<<dstr(x.first)<<" / ";
		printterm(*x.second);
		dout << ";";
	}
//	dout << "] ";
}

void printl(const termset& l) {
	for (auto x : l) {
		printterm(*x);
		dout << ',';
	}
}

void printr(const rule& r) {
	printl(r.body);
	dout<<" => ";
	printterm(*r.p);
}

void printg(const ground& g) {
	for (auto x : g) {
		printr(*x.first);
		dout<<':';
		prints(x.second);
	}
	dout<<'.';
}

void printe(const evidence& e) {
	for (auto y : e)
		for (auto x : y.second) {
			printterm(*x.first);
			dout << ": ";
			printg(x.second);
			dout << endl;
#ifdef IRC
			sleep(1);
#endif
		}
}
}
