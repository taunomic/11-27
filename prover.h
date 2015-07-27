#ifndef __PROVER_H__
#define __PROVER_H__

/*
    Created on: Apr 28, 2015
        Author: Ohad Asor
*/

#include <deque>
#include <climits>
#include "rdf.h"
#include "misc.h"
#include <future>
#include <functional>
#include <forward_list>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/containers/set.hpp>
#include <boost/interprocess/containers/vector.hpp>

typedef u64 termid;
typedef boost::container::map<resid, termid> subst;

class prover {
public:
	class term {
	public:
		term();
		term(resid _p, termid _s, termid _o);
		resid p;
		termid s, o;
		pobj json(const prover&) const;
	};
	typedef u64 ruleid;
	typedef boost::container::vector<termid> termset;
	class ruleset {
	public:
		typedef boost::container::vector<termset> btype;
	private:
		termset _head;
		btype  _body;
		size_t m = 0;
	public:
		prover* p;
		ruleset(prover* _p) : p(_p) {}
		ruleid add(termid t, const termset& ts);
		ruleid add(termid t);
		const termset& head() const	{ return _head; }
		const btype& body() const	{ return _body; }
		size_t size()			{ return _head.size(); }
		void mark();
		void revert();
		typedef std::vector<ruleid> rulelist;
		typedef boost::container::map<resid, rulelist> r2id_t;
		string format() const;
		inline const rulelist& operator[](resid id) const { return r2id.at(id); }
		r2id_t r2id;
	} kb;
	prover ( qdb, bool check_consistency = true);
	prover ( ruleset* kb = 0 );
	prover ( string filename );
	prover ( const prover& p );
	void operator()(termset& goal, subst* s = 0);
	void operator()(const qdb& goal, subst* s = 0);
	const term& get(termid) const;
	const term& get(resid) const { throw std::runtime_error("called get(termid) with resid"); }
	~prover();

	typedef boost::container::list<std::pair<ruleid, shared_ptr<subst>>> ground;
	typedef boost::container::map<resid, boost::container::set<std::pair<termid, ground>>> evidence;
	evidence e;
	std::vector<subst> substs;
	termid tmpvar();
	void printg(const ground& g);
	void printg(shared_ptr<ground> g) { printg(*g); }
	void printe();
	termid make(pnode p, termid s = 0, termid o = 0);
	termid make(resid p, termid s = 0, termid o = 0);
	termid make(termid) { throw std::runtime_error("called make(pnode/resid) with termid"); }
	termid make(termid, termid, termid) { throw std::runtime_error("called make(pnode/resid) with termid"); }
	string format(const termset& l, bool json = false);
	void prints(const subst& s);
	void prints(shared_ptr<subst> s) { prints(*s); }

	struct proof {
		ruleid rul = 0;
		uint last;
		shared_ptr<proof> prev = 0;
		shared_ptr<subst> s;
		ground g;
		proof() : s(make_shared<subst>()) {}
		proof(ruleid r, uint l = 0, shared_ptr<proof> p = 0, const subst& _s = subst(), const ground& _g = ground() ) 
			: rul(r), last(l), prev(p), s(make_shared<subst>(_s)), g(_g) {} 
		proof(const proof& p) : rul(p.rul), last(p.last), prev(p.prev), s(make_shared<subst>(*p.s)), g(p.g) { }
		proof(const proof& p, const ground& _g) : rul(p.rul), last(p.last), prev(p.prev), g(_g) { }
	};
	typedef std::deque<shared_ptr<proof>> queue_t;

	void addrules(pquad q, qdb& quads);
	std::vector<termid> get_list(termid head, proof& p);
	termid list2term(std::list<pnode>& l, const qdb& quads);
	termid list2term_simple(std::list<termid>& l);
	string format(termid id, bool json = false);
	void get_dotstyle_list(termid, std::list<resid>&);
	string formatkb(bool json = false);

private:

	class termdb {
		typedef boost::container::vector<term> terms_t;
	public:
		terms_t terms;
		typedef boost::container::vector<termid> termlist;
		typedef boost::container::map<resid, termlist> p2id_t;
		size_t size() const { return terms.size(); }
		inline const term& operator[](termid id) const { return terms.at(id); }
		inline const termlist& operator[](resid id) const { return p2id.at(id); }
		inline termid add(resid p, termid s, termid o) { terms.emplace_back(p, s, o); termid r = size(); p2id[p].push_back(r); return r; }
	private:
		p2id_t p2id;
	} _terms;
	friend ruleset;
	int steps = 0;

	void pushev(shared_ptr<proof>);
	void step(shared_ptr<proof>&, queue_t&, queue_t&);
	termid evaluate(termid id);
	termid evaluate(termid id, const subst& s);
	inline termid evaluate(termid id, shared_ptr<subst>& s) { return s ? evaluate(id, *s) : evaluate(id); }
	bool unify(termid _s, const subst& ssub, termid _d, subst& dsub, bool f);
	inline bool unify(termid _s, shared_ptr<subst>& ssub, termid _d, shared_ptr<subst>& dsub, bool f) { return unify(_s, *ssub, _d, *dsub, f); }
	bool euler_path(shared_ptr<proof>&);
	int builtin(termid, shared_ptr<proof>, queue_t&);
	termid quad2term(const quad& p, const qdb& quads);
	termid list_next(termid t, proof&);
	termid list_first(termid t, proof&);
	bool islist(termid);
	bool consistency(const qdb& quads);

	// formatters
	string format(resid) { throw std::runtime_error("called format(termid) with resid"); }
	string format(resid, bool) { throw std::runtime_error("called format(termid) with resid"); }
	string format(term t, bool json = false);
	string formatr(ruleid r, bool json = false);
	string formatg(const ground& g, bool json = false);
	void printp(shared_ptr<proof> p);
	string formats(const subst& s, bool json = false);
	string formats(shared_ptr<subst>& s, bool json = false) { return s ? formats(*s, json) : string(); }
	void printterm_substs(termid id, const subst& s);
	void printl_substs(const termset& l, const subst& s);
	void printr_substs(ruleid r, const subst& s);
	void jprinte();
	pobj json(const termset& ts) const;
	pobj json(const subst& ts) const;
	pobj json(const ground& g) const;
	pobj json(ruleid rl) const;
	pobj ejson() const;
};
#endif
