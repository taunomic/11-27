#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <utility>
#include <memory>
#include <list>
#include <boost/variant.hpp>
#include "json_spirit_reader.h"
#include "json_spirit_writer.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/easy.h>
#include <curl/curlbuild.h>
#include <sstream>
#include <iostream>

using namespace std;
using namespace std::string_literals;
using namespace boost;

#include "err.h"
#include "util.h"

typedef nullptr_t null;

class obj {
protected:
	obj() {}
public:
	typedef std::shared_ptr<obj> pobj;
	typedef map<string, pobj> somap;
	typedef vector<pobj> olist;
	typedef std::shared_ptr<somap> psomap;

	virtual std::shared_ptr<uint64_t> UINT() {
		return 0;
	}
	virtual std::shared_ptr<int64_t> INT() {
		return 0;
	}
	virtual std::shared_ptr<bool> BOOL() {
		return 0;
	}
	virtual std::shared_ptr<string> STR() {
		return 0;
	}
	virtual std::shared_ptr<somap> MAP() {
		return 0;
	}
	virtual std::shared_ptr<olist> LIST() {
		return 0;
	}
	virtual std::shared_ptr<double> DOUBLE() {
		return 0;
	}
	virtual std::shared_ptr<null> Null() {
		return 0;
	}

	bool map_and_has ( const string& k ) {
		psomap m = MAP();
		return m && m->find ( k ) != m->end();
	}
	bool map_and_has_null ( const string& k ) {
		psomap m = MAP();
		if ( !m ) return false;
		auto it = m->find ( k );
		return it != m->end() && it->second->Null();
	}
	virtual string type_str() const = 0;
	virtual bool equals ( const obj& o ) const = 0;
};

#define OBJ_IMPL(type, getter) \
class type##_obj : public obj { \
	std::shared_ptr<type> data; \
public: \
	type##_obj(const type& o = type()) { data = make_shared<type>(); *data = o; } \
	type##_obj(const std::shared_ptr<type> o) : data(o) { } \
	virtual std::shared_ptr<type> getter() { return data; } \
	virtual string type_str() const { return #type; } \
	virtual bool equals(const obj& o) const { \
		if ( type_str() != o.type_str() ) return false; \
		auto od = ((const type##_obj&)o).data; \
		if ( !data || !od) return data == od; \
		return *data == *od; \
	}\
}

OBJ_IMPL ( int64_t, INT );
OBJ_IMPL ( uint64_t, UINT );
OBJ_IMPL ( bool, BOOL );
OBJ_IMPL ( double, DOUBLE );
OBJ_IMPL ( string, STR );
OBJ_IMPL ( somap, MAP );
OBJ_IMPL ( olist, LIST );
OBJ_IMPL ( null, Null );

typedef obj::pobj pobj;
typedef obj::somap somap;
typedef obj::olist olist;
typedef std::shared_ptr<string> pstring;
typedef std::shared_ptr<somap> psomap;
typedef std::shared_ptr<bool> pbool;
typedef map<string, bool> defined_t;
typedef std::shared_ptr<defined_t> pdefined_t;

string resolve ( const string&, const string& ) {
	return "";
}

#include "loader.h"

bool equals ( const obj& a, const obj& b ) {
	return a.equals ( b );
}
bool equals ( const pobj& a, const pobj& b ) {
	return a->equals ( *b );
}
bool equals ( const pobj& a, const obj& b ) {
	return a->equals ( b );
}
bool equals ( const obj& a, const pobj& b ) {
	return a.equals ( *b );
}

// http://www.w3.org/TR/json-ld-api/#the-jsonldoptions-type
struct jsonld_options {
	jsonld_options() {}
	jsonld_options ( string base_ ) : base ( make_shared<string> ( base_ ) ) {}
	pstring base = 0;
	pbool compactArrays = make_shared<bool> ( true );
	pobj expandContext = 0;
	pstring processingMode = make_shared<string> ( "json-ld-1.0" );
	pbool embed = 0;
	pbool isexplicit = 0;
	pbool omitDefault = 0;
	pbool useRdfType = make_shared<bool> ( false );
	pbool useNativeTypes = make_shared<bool> ( false );
	pbool produceGeneralizedRdf = make_shared<bool> ( false );
	pstring format = 0;
	pbool useNamespaces = make_shared<bool> ( false );
	pstring outputForm = 0;
};

bool keyword ( const string& key ) {
	return "@base"s == key || "@context"s == key || "@container"s == key
	       || "@default"s == key || "@embed"s == key || "@explicit"s == key
	       || "@graph"s == key || "@id"s == key || "@index"s == key
	       || "@language"s == key || "@list"s == key || "@omitDefault"s == key
	       || "@reverse"s == key || "@preserve"s == key || "@set"s == key
	       || "@type"s == key || "@value"s == key || "@vocab"s == key;
}

bool keyword ( pobj p ) {
	if ( !p || !p->STR() ) return false;
	return keyword ( *p->STR() );
}

bool is_abs_iri ( const string& s ) {
	return s.find ( ':' ) != string::npos;
}
bool is_rel_iri ( const string& s ) {
	return !keyword ( s ) || !is_abs_iri ( s );
}
pobj newMap ( const string& k, pobj v ) {
	pobj r = make_shared<somap_obj>();
	( *r->MAP() ) [k] = v;
	return r;
}

//template<typename obj>
class Context : public somap_obj {
private:
	jsonld_options options;
	psomap term_defs = make_shared<somap>();
public:
	psomap inverse = 0;

	Context ( const jsonld_options& o = jsonld_options() ) : somap_obj(), options ( o ) {
		if ( options.base ) MAP()->at ( "@base" ) = make_shared<string_obj> ( *options.base );
	}

	pstring getContainer ( string prop ) {
		if ( prop == "@graph" ) return make_shared<string> ( "@set" );
		if ( keyword ( prop ) ) return make_shared<string> ( prop );
		auto it = term_defs->find ( prop );
		return it == term_defs->end() ? 0 : it->second->STR();
	}

	//Context Processing Algorithm http://json-ld.org/spec/latest/json-ld-api/#context-processing-algorithms
	Context parse ( pobj localContext, vector<string>& remoteContexts ) {
		Context result ( *this );
		if ( !localContext->LIST() ) localContext = make_shared<olist_obj> ( olist ( 1, localContext ) );
		for ( auto context : *localContext->LIST() ) {
			if ( context->Null() ) result = Context ( options );
			else if ( pstring s = context->STR() ) {
				string uri = resolve ( * ( *result.MAP() ) ["@base"]->STR(), *s ); // REVISE
				if ( std::find ( remoteContexts.begin(), remoteContexts.end(), uri ) != remoteContexts.end() ) throw RECURSIVE_CONTEXT_INCLUSION + "\t" + uri;
				remoteContexts.push_back ( uri );

				pobj remoteContext = fromURL ( uri );
				somap* p;
				if ( !remoteContext->map_and_has ( "@context" ) ) throw INVALID_REMOTE_CONTEXT + "\t"; // + context;
				context = ( *remoteContext->MAP() ) ["@context"];
				result = result.parse ( context, remoteContexts );
				continue;
			} else if ( !context->MAP() ) throw INVALID_LOCAL_CONTEXT + "\t"; // + context;
			somap& cm = *context->MAP();
			auto it = cm.find ( "@base" );
			if ( !remoteContexts.size() && it != cm.end() ) {
				pobj value = it->second;
				if ( value->Null() ) result.MAP()->erase ( "@base" );
				else if ( pstring s = value->STR() ) {
					if ( is_abs_iri ( *s ) ) ( *result.MAP() ) ["@base"] = value;
					else {
						string baseUri = * ( *result.MAP() ) ["@base"]->STR();
						if ( !is_abs_iri ( baseUri ) ) throw INVALID_BASE_IRI + "\t" + baseUri;
						( *result.MAP() ) ["@base"] = make_shared<string_obj> ( resolve ( baseUri, *s ) );
					}
				} else throw INVALID_BASE_IRI + "\t" + "@base must be a string";
			}
			// 3.5
			if ( ( it = cm.find ( "@vocab" ) ) != cm.end() ) {
				pobj value = it->second;
				if ( value->Null() ) result.MAP()->erase ( it );
				else if ( pstring s = value->STR() ) {
					if ( is_abs_iri ( *s ) ) ( *result.MAP() ) ["@vocab"] = value;
					else throw INVALID_VOCAB_MAPPING + "\t" + "@value must be an absolute IRI";
				} else throw INVALID_VOCAB_MAPPING + "\t" + "@vocab must be a string or null";
			}
			if ( ( it = cm.find ( "@language" ) ) != cm.end() ) {
				pobj value = it->second;
				if ( value->Null() ) result.MAP()->erase ( it );
				else if ( pstring s = value->STR() ) ( *result.MAP() ) ["@language"] = make_shared<string_obj> ( lower ( *s ) );
				else throw INVALID_DEFAULT_LANGUAGE + "\t";// + value;
			}
			pdefined_t defined = make_shared<defined_t>();
			for ( auto it : cm ) {
				if ( is ( it.first, { "@base"s, "@vocab"s, "@language"s } ) ) continue;
				result.createTermDefinition ( context->MAP(), it.first, defined ); // REVISE
			}
		}
		return result;
	}
	// Create Term Definition Algorithm
	void createTermDefinition ( const psomap context, const string term, pdefined_t pdefined ) {
		defined_t& defined = *pdefined;
		if ( defined.find ( term ) != defined.end() ) {
			if ( defined[term] ) return;
			throw CYCLIC_IRI_MAPPING + "\t" + term;
		}
		defined[term] = false;
		if ( keyword ( term ) ) throw KEYWORD_REDEFINITION + "\t" + term;
		term_defs->erase ( term );
		auto it = context->find ( term );
		psomap m;
		decltype ( m->end() ) _it;
		if ( it == context->end() || it->second->map_and_has_null ( "@id" ) ) {
			( *term_defs ) [term] = make_shared<null_obj>();
			defined[term] = true;
			return;
		}
		pobj& value = it->second;
		if ( value->STR() ) value = newMap ( "@id", value );
		if ( value->MAP() ) throw INVALID_TERM_DEFINITION;
		somap defn, &val = *value->MAP();
		//10
		if ( ( it = val.find ( "@type" ) ) != val.end() ) {
			pstring type = it->second->STR();
			if ( !type ) throw INVALID_TYPE_MAPPING;
			type = expandIri ( type, false, true, context, pdefined );
			if ( is ( *type, {"@id"s, "@vocab"s} ) || ( !startsWith ( *type, "_:" ) && is_abs_iri ( *type ) ) ) defn["@type"] = make_shared<string_obj> ( *type );
			else throw INVALID_TYPE_MAPPING + "\t" + *type;
		}
		// 11
		if ( ( it = val.find ( "@reverse" ) ) != val.end() ) {
			if ( throw_if_not_contains ( val, "@id", INVALID_REVERSE_PROPERTY ) && !it->second->STR() )
				throw INVALID_IRI_MAPPING + "\t" + "Expected String for @reverse value.";
			string reverse = *expandIri ( val.at ( "@reverse" )->STR(), false, true, context, pdefined );
			if ( !is_abs_iri ( reverse ) ) throw INVALID_IRI_MAPPING + "Non-absolute @reverse IRI: " + reverse;
			defn ["@id"] = make_shared<string_obj> ( reverse );
			if ( ( it = val.find ( "@container" ) ) != val.end() && is ( *it->second->STR(), { "@set"s, "@index"s }, INVALID_REVERSE_PROPERTY + "reverse properties only support set- and index-containers" ) )
				defn ["@container"] = it->second;
			defn["@reverse"] = make_shared<bool_obj> ( ( *pdefined ) [term] = true );
			( *term_defs ) [term] = make_shared<somap_obj> ( defn );
			return;
		}
		defn["@reverse"] = make_shared<bool_obj> ( false );
		size_t colIndex;
		if ( ( it = val.find ( "@id" ) ) != val.end() && it->second->STR() && *it->second->STR() != term ) {
			if ( ! it->second->STR() ) throw INVALID_IRI_MAPPING + "expected value of @id to be a string";
			pstring res = expandIri ( it->second->STR(), false, true, context, pdefined );
			if ( res && keyword ( *res ) || is_abs_iri ( *res ) ) {
				if ( *res == "@context" ) throw INVALID_KEYWORD_ALIAS + "cannot alias @context";
				defn ["@id"] = make_shared<string_obj> ( res );
			} else throw INVALID_IRI_MAPPING + "resulting IRI mapping should be a keyword, absolute IRI or blank node";
		} else if ( ( colIndex = term.find ( ":" ) ) != string::npos ) {
			string prefix = term.substr ( 0, colIndex );
			string suffix = term.substr ( colIndex + 1 );
			if ( context->find ( prefix ) != context->end() ) createTermDefinition ( context, prefix, pdefined );
			if ( ( it = term_defs->find ( prefix ) ) != term_defs->end() )
				defn ["@id"] = make_shared<string_obj> ( *it->second->MAP()->at ( "@id" )->STR() + suffix );
			else defn["@id"] = make_shared<string_obj> ( term );
		} else if ( ( it = MAP()->find ( "@vocab" ) ) != MAP()->end() )
			defn ["@id"] = make_shared<string_obj> ( *MAP()->at ( "@vocab" )->STR() + term );
		else throw INVALID_IRI_MAPPING + "relative term defn without vocab mapping";

		// 16
		bool tmp = ( ( it = val.find ( "@container" ) ) != val.end() ) && it->second->STR() &&
		           is ( *it->second->STR(), { "@list"s, "@set"s, "@index"s, "@language"s }, INVALID_CONTAINER_MAPPING + "@container must be either @list, @set, @index, or @language" ) && ( defn["@container"] = it->second );

		auto i1 = val.find ( "@language" ), i2 = val.find ( "type" );
		pstring lang;
		if ( i1 != val.end() && i2 == val.end() ) {
			if ( !i1->second->Null() || ( lang = i2->second->STR() ) ) defn["@language"] = lang ? make_shared<string_obj> ( lower ( *lang ) ) : 0;
			else throw INVALID_LANGUAGE_MAPPING + "@language must be a string or null";
		}

		( *term_defs ) [term] = make_shared<somap_obj> ( defn );
		( *pdefined ) [term] = true;
	}


	//http://json-ld.org/spec/latest/json-ld-api/#iri-expansion
	pstring expandIri ( const pstring value, bool relative, bool vocab, const psomap context, pdefined_t pdefined ) {
		if ( !value || keyword ( *value ) ) return value;
		const defined_t& defined = *pdefined;
		if ( context && has ( context, value ) && !defined.at ( *value ) ) createTermDefinition ( context, *value, pdefined );
		if ( vocab && has ( term_defs, value ) ) {
			psomap td;
			return ( td = term_defs->at ( *value )->MAP() ) ? td->at ( "@id" )->STR() : 0;
		}
		size_t colIndex = value->find ( ":" );
		if ( colIndex != string::npos ) {
			string prefix = value->substr ( 0, colIndex ), suffix = value->substr ( colIndex + 1 );
			if ( prefix == "_" || startsWith ( suffix, "//" ) ) return value;
			if ( context && has ( context, prefix ) && ( !has ( pdefined, prefix ) || !defined.at ( prefix ) ) )
				createTermDefinition ( context, prefix, pdefined );
			if ( has ( term_defs, prefix ) ) return make_shared<string> ( *term_defs->at ( prefix )->MAP()->at ( "@id" )->STR() + suffix );
			return value;
		}
		if ( vocab && MAP()->find ( "@vocab" ) != MAP()->end() ) return make_shared<string> ( *MAP()->at ( "@vocab" )->STR() + *value );
		if ( relative ) return make_shared<string> ( resolve ( *MAP()->at ( "@base" )->STR(), *value ) );
		if ( context && is_rel_iri ( *value ) ) throw INVALID_IRI_MAPPING + "not an absolute IRI: " + *value;
		return value;
	}

	pstring get_type_map ( const string& prop ) {
		auto td = term_defs->find ( prop );
		return td == term_defs->end() || !td->second->MAP() ? 0 : td->second->MAP()->at ( "@type" )->STR();
	}

	pstring get_lang_map ( const string& prop ) {
		auto td = term_defs->find ( prop );
		return td == term_defs->end() || !td->second->MAP() ? 0 : td->second->MAP()->at ( "@language" )->STR();
	}

	psomap get_term_def ( const string& key ) {
		return term_defs->at ( key )->MAP();
	}

	// http://json-ld.org/spec/latest/json-ld-api/#iri-compaction
	pstring compactIri ( pstring iri, pobj value, bool relativeToVocab, bool reverse ) {
		if ( !iri ) return 0;
		if ( relativeToVocab && getInverse().containsKey ( iri ) ) {
			auto it = MAP()->find ( "@language" );
			pstring defaultLanguage = 0;
			if ( it != MAP()->end() && it->second ) defaultLanguage = it->second->STR();
			if ( !defaultLanguage ) defaultLanguage = make_shared<string> ( "@none" );
			vector<string> containers;
			string type_lang = "@language", type_lang_val = "@null";
			if ( value->MAP() && has ( value, "@index" ) ) containers.push_back ( "@index" );
			if ( reverse ) {
				type_lang = "@type";
				type_lang_val = "@reverse";
				containers.push_back ( "@set" );
			} else if ( value->MAP() && has ( value->MAP(),  "@list" ) ) {
				if ( ! has ( value->MAP(),  "@index" ) )
					containers.push_back ( "@list" );
				// 2.6.2)
				final List<Object> list = ( List<Object> ) value->MAP( ).at ( "@list" );
				// 2.6.3)
				pstring common_lang = ( list.size() == 0 ) ? defaultLanguage : 0, common_type = 0;
				// 2.6.4)
				for ( pobj item : list ) {
					string itemLanguage = "@none", itemType = "@none";
					if ( isvalue ( item ) ) {
						if ( ( it = item->MAP()->find ( "@language" ) ) != item->MAP()->end() ) itemLanguage = it->second->STR();
						else if (  ( it = item->MAP()->find ( "@type" ) ) != item->MAP()->end()  ) itemType = it->second->STR();
						else itemLanguage = "@null";
					} else itemType = "@id";
					if ( !common_lang ) common_lang = itemLanguage;
					else if ( !common_lang.equals ( itemLanguage ) && JsonLdUtils.isValue ( item ) ) common_lang = "@none";
					if ( common_type == null ) common_type = itemType;
					else if ( !common_type.equals ( itemType ) ) common_type = "@none";
					if ( "@none"s == ( common_lang ) && "@none"s == ( common_type ) ) break;
				}
				common_lang = ( common_lang != null ) ? common_lang : "@none";
				common_type = ( common_type != null ) ? common_type : "@none";
				if ( !"@none"s == ( common_type ) ) {
					type_lang = "@type";
					type_lang_val = common_type;
				} else type_lang_val = common_lang;
			} else {
				if ( value->MAP() && has ( value->MAP(),  "@value" ) ) {
					if ( has ( value->MAP(),  "@language" )
					        && ! has ( value->MAP(),  "@index" ) ) {
						containers.push_back ( "@language" );
						type_lang_val = ( String ) value->MAP( ).at ( "@language" );
					} else if ( has ( value->MAP(),  "@type" ) ) {
						type_lang = "@type";
						type_lang_val = ( String ) value->MAP( ).at ( "@type" );
					}
				} else {
					type_lang = "@type";
					type_lang_val = "@id";
				}
				containers.push_back ( "@set" );
			}

			containers.push_back ( "@none" );
			if ( !type_lang_val ) type_lang_val = make_shared<string> ( "@null" );
			vector<string> preferredValues;
			if ( "@reverse"s ==  *type_lang_val  ) preferredValues.add ( "@reverse" );
			// 2.12)
			if ( ( "@reverse"s ==  type_lang_val  || "@id"s ==  type_lang_val  )
			        && ( value->MAP() ) && has ( value->MAP(),  "@id" ) ) {
				// 2.12.1)
				final String result = this.compactIri (
				                          ( String ) value->MAP( ).at ( "@id" ), null, true, true );
				if ( termDefinitions.containsKey ( result )
				        && ( ( Map<String, Object> ) termDefinitions.get ( result ) ).containsKey ( "@id" )
				        && value->MAP( ).at ( "@id" ).equals (
				            ( ( Map<String, Object> ) termDefinitions.get ( result ) ).get ( "@id" ) ) ) {
					preferredValues.add ( "@vocab" );
					preferredValues.add ( "@id" );
				}
				// 2.12.2)
				else {
					preferredValues.add ( "@id" );
					preferredValues.add ( "@vocab" );
				}
			}
			// 2.13)
			else
				preferredValues.add ( type_lang_val );
			preferredValues.add ( "@none" );

			// 2.14)
			final String term = selectTerm ( iri, containers, type_lang, preferredValues );
			// 2.15)
			if ( term != null )
				return term;
		}

		// 3)
		if ( relativeToVocab && this.containsKey ( "@vocab" ) ) {
			// determine if vocab is a prefix of the iri
			final String vocab = ( String ) this.get ( "@vocab" );
			// 3.1)
			if ( iri.indexOf ( vocab ) == 0 && !iri.equals ( vocab ) ) {
				// use suffix as relative iri if it is not a term in the
				// active context
				final String suffix = iri.substring ( vocab.length() );
				if ( !termDefinitions.containsKey ( suffix ) )
					return suffix;
			}
		}

		// 4)
		String compactIRI = null;
		// 5)
		for ( final String term : termDefinitions.keySet() ) {
			final Map<String, Object> termDefinition = ( Map<String, Object> ) termDefinitions
			        .get ( term );
			// 5.1)
			if ( term.contains ( ":" ) )
				continue;
			// 5.2)
			if ( termDefinition == null || iri.equals ( termDefinition.get ( "@id" ) )
			        || !iri.startsWith ( ( String ) termDefinition.get ( "@id" ) ) )
				continue;

			// 5.3)
			final String candidate = term + ":"
			                         + iri.substring ( ( ( String ) termDefinition.get ( "@id" ) ).length() );
			// 5.4)
			if ( ( compactIRI == null || compareShortestLeast ( candidate, compactIRI ) < 0 )
			        && ( !termDefinitions.containsKey ( candidate ) || ( iri
			                .equals ( ( ( Map<String, Object> ) termDefinitions.get ( candidate ) )
			                          .get ( "@id" ) ) && value == null ) ) )
				compactIRI = candidate;

		}

		// 6)
		if ( compactIRI != null )
			return compactIRI;

		// 7)
		if ( !relativeToVocab )
			return JsonLdUrl.removeBase ( this.get ( "@base" ), iri );

		// 8)
		return iri;
	}

	// http://json-ld.org/spec/latest/json-ld-api/#value-compaction
	pobj compactValue ( pstring activeProperty, somap_obj value_ ) {
		psomap value = value_.MAP();
		int nvals = value->size();
		auto p = getContainer ( *activeProperty );
		if ( value->find ( "@index" ) != value->end() && p && *p == "@index" ) nvals--;
		if ( nvals > 2 ) return make_shared<somap_obj> ( value_ );
		pstring type_map = get_type_map ( *activeProperty ), lang_map = get_lang_map ( *activeProperty );
		auto it = value->find ( "@id" );
		if ( it != value->end() && nvals == 1 )
			return type_map && *type_map == "@id" && nvals == 1 ? compactIri ( value->at ( "@id" )->STR() ) : type_map && *type_map == "@vocab" && nvals == 1 ? compactIri ( value->at ( "@id" )->STR(), true ) : value;
		pobj valval = value->at ( "@value" );
		auto it = value->find ( "@type" );
		if ( it != value->end() && equals ( it->second, type_map ) ) return valval;
		if ( ( it = value->find ( "@language" ) ) != value->end() ) // TODO: SPEC: doesn't specify to check default language as well
			if ( equals ( it->second, lang_map ) || equals ( it->second, MAP()->at ( "@language" ) ) )
				return valval;
		if ( nvals == 1
		        && ( !valval->STR() || has ( MAP(), "@language" ) || ( term_defs.find ( activeProperty ) == term_defs.end()
		                && has ( getTermDefinition ( activeProperty ), "@language" ) && lang_map == null ) ) )
			return valval;
		return value;
	}
};

int main() {
	Context c;
	vector<string> v;
	c.parse ( 0, v );
	return 0;
}