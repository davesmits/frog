/*
  $Id$
  $URL$

  Copyright (c) 2006 - 2015
  Tilburg University

  This file is part of frog.

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/

#include <cstdlib>
#include <string>
#include <set>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include "timbl/TimblAPI.h"

#include "ucto/unicode.h"
#include "ticcutils/Configuration.h"
#include "frog/Frog.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace folia;
using namespace TiCC;

const long int LEFT =  6; // left context
const long int RIGHT = 6; // right context

Mbma::Mbma(LogStream * logstream):
  MTreeFilename( "dm.igtree" ),
  MTree(0),
  transliterator(0),
  filter(0),
  doDaring(false)
{
  mbmaLog = new LogStream( logstream, "mbma-" );
}

Mbma::~Mbma() {
  cleanUp();
  delete transliterator;
  delete filter;
  delete mbmaLog;
}


void Mbma::init_cgn( const string& main, const string& sub ) {
  ifstream tc( main );
  if ( tc ){
    string line;
    while( getline( tc, line) ) {
      vector<string> tmp;
      size_t num = split_at(line, tmp, " ");
      if ( num < 2 ){
	*Log(mbmaLog) << "splitting '" << line << "' failed" << endl;
	throw ( runtime_error("panic") );
      }
      TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
    }
  }
  else
    throw ( runtime_error( "unable to open:" + sub ) );
  ifstream tc1( sub );
  if ( tc1 ){
    string line;
    while( getline(tc1, line) ) {
      vector<string> tmp;
      size_t num = split_at(line, tmp, " ");
      if ( num == 2 )
	TAGconv.insert( make_pair( tmp[0], tmp[1] ) );
    }
  }
  else
    throw ( runtime_error( "unable to open:" + sub ) );
}

Transliterator *Mbma::init_trans( ){
  UErrorCode stat = U_ZERO_ERROR;
  Transliterator *t = Transliterator::createInstance( "NFD; [:M:] Remove; NFC",
						      UTRANS_FORWARD,
						      stat );
  if ( U_FAILURE( stat ) ){
    throw runtime_error( "initFilter FAILED !" );
  }
  return t;
}

bool Mbma::init( const Configuration& config ) {
  *Log(mbmaLog) << "Initiating morphological analyzer..." << endl;
  debugFlag = 0;
  string val = config.lookUp( "debug", "mbma" );
  if ( val.empty() ){
    val = config.lookUp( "debug" );
  }
  if ( !val.empty() ){
    debugFlag = TiCC::stringTo<int>( val );
  }
  val = config.lookUp( "daring", "mbma" );
  if ( !val.empty() )
    doDaring = TiCC::stringTo<bool>( val );
  val = config.lookUp( "version", "mbma" );
  if ( val.empty() ){
    version = "1.0";
  }
  else
    version = val;
  val = config.lookUp( "set", "mbma" );
  if ( val.empty() ){
    mbma_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";
  }
  else
    mbma_tagset = val;

  val = config.lookUp( "set", "tagger" );
  if ( val.empty() ){
    cgn_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
  }
  else
    cgn_tagset = val;

  val = config.lookUp( "clex_set", "mbma" );
  if ( val.empty() ){
    clex_tagset = "http://ilk.uvt.nl/folia/sets/frog-mbpos-clex";
  }
  else
    clex_tagset = val;

  string cgn_clex_main;
  val = config.lookUp( "cgn_clex_main", "mbma" );
  if ( val.empty() ){
    cgn_clex_main = "cgntags.main";
  }
  else
    cgn_clex_main = val;
  cgn_clex_main = prefix( config.configDir(), cgn_clex_main );

  string cgn_clex_sub;
  val = config.lookUp( "cgn_clex_sub", "mbma" );
  if ( val.empty() ){
    cgn_clex_sub = "cgntags.sub";
  }
  else
    cgn_clex_sub = val;
  cgn_clex_sub = prefix( config.configDir(), cgn_clex_sub );
  init_cgn( cgn_clex_main, cgn_clex_sub );

  string charFile = config.lookUp( "char_filter_file", "mbma" );
  if ( charFile.empty() )
    charFile = config.lookUp( "char_filter_file" );
  if ( !charFile.empty() ){
    charFile = prefix( config.configDir(), charFile );
    filter = new Tokenizer::UnicodeFilter();
    filter->fill( charFile );
  }
  string tfName = config.lookUp( "treeFile", "mbma" );
  if ( tfName.empty() )
    tfName = "mbma.igtree";
  MTreeFilename = prefix( config.configDir(), tfName );
  string dof = config.lookUp( "filter_diacritics", "mbma" );
  if ( !dof.empty() ){
    bool b = stringTo<bool>( dof );
    if ( b ){
      transliterator = init_trans();
    }
  }
  //Read in (igtree) data
  string opts = config.lookUp( "timblOpts", "mbma" );
  if ( opts.empty() )
    opts = "-a1";
  opts += " +vs -vf"; // make Timbl run quietly
  MTree = new Timbl::TimblAPI(opts);
  return MTree->GetInstanceBase(MTreeFilename);
}

void Mbma::cleanUp(){
  // *Log(mbmaLog) << "cleaning up MBMA stuff " << endl;
  delete MTree;
  MTree = 0;
  clearAnalysis();
}

vector<string> Mbma::make_instances( const UnicodeString& word ){
  vector<string> insts;
  insts.reserve( word.length() );
  for( long i=0; i < word.length(); ++i ) {
    if (debugFlag > 10)
      *Log(mbmaLog) << "itt #:" << i << endl;
    UnicodeString inst;
    for ( long j=i ; j <= i + RIGHT + LEFT; ++j ) {
      if (debugFlag > 10)
	*Log(mbmaLog) << " " << j-LEFT << ": ";
      if ( j < LEFT || j >= word.length()+LEFT )
	inst += '_';
      else {
	if (word[j-LEFT] == ',' )
	  inst += 'C';
	else
	  inst += word[j-LEFT];
      }
      inst += ",";
      if (debugFlag > 10)
	*Log(mbmaLog) << " : " << inst << endl;
    }
    inst += "?";
    insts.push_back( UnicodeToUTF8(inst) );
  }
  return insts;
}

string find_class( unsigned int step,
		   const vector<string>& classes,
		   unsigned int nranal ){
  string result = classes[0];
  if ( nranal > 1 ){
    if ( classes.size() > 1 ){
      if ( classes.size() > step )
	result = classes[step];
      else
	result = "0";
    }
  }
  return result;
}

#define OLD
#ifdef OLD
vector<vector<string> > generate_all_perms( const vector<string>& classes ){
  // determine all alternative analyses, remember the largest
  // and store every part in a vector of string vectors
  int largest_anal=1;
  vector<vector<string> > classParts;
  classParts.reserve( classes.size() );
  for ( const auto& cl : classes ){
    vector<string> parts;
    int num = split_at( cl, parts, "|" );
    if ( num > 0 ){
      classParts.push_back( parts );
      if ( num > largest_anal )
	largest_anal = num;
    }
    else {
      // only one, create a dummy
      vector<string> dummy;
      dummy.push_back( cl );
      classParts.push_back( dummy );
    }
  }
  //
  // now expand the result
  vector<vector<string> > result;
  result.reserve( largest_anal );
  for ( int step=0; step < largest_anal; ++step ){
    vector<string> item(classParts.size());
    for ( size_t k=0; k < classParts.size(); ++k ) {
      item[k] = find_class( step, classParts[k], largest_anal );
    }
    result.push_back( item );
  }
  return result;
}
#else

bool next_perm( vector< vector<string>::const_iterator >& its,
		const vector<vector<string> >& parts ){
  for ( size_t i=0; i < parts.size(); ++i ){
    ++its[i];
    if ( its[i] == parts[i].end() ){
      if ( i == parts.size() -1 )
	return false;
      its[i] = parts[i].begin();
    }
    else
      return true;
  }
  return false;
}

vector<vector<string> > generate_all_perms( const vector<string>& classes ){

  // determine all alternative analyses
  // store every part in a vector of string vectors
  vector<vector<string> > classParts;
  classParts.resize( classes.size() );
  for ( unsigned int j=0; j< classes.size(); ++j ){
    vector<string> parts;
    int num = split_at( classes[j], parts, "|" );
    if ( num > 0 ){
     classParts[j] = parts;
    }
    else {
      // only one, create a dummy
      vector<string> dummy;
      dummy.push_back( classes[j] );
      classParts[j] = dummy;
   }
  }
  //
  // now expand
  vector< vector<string>::const_iterator > its( classParts.size() );
  for ( size_t i=0; i<classParts.size(); ++i ){
    its[i] = classParts[i].begin();
  }
  vector<vector<string> > result;
  bool more = true;
  while ( more ){
    vector<string> item(classParts.size());
    for( size_t j=0; j< classParts.size(); ++j ){
      item[j] = *its[j];
    }
    result.push_back( item );
    more = next_perm( its, classParts );
  }
  return result;
}
#endif

void Mbma::clearAnalysis(){
  for ( const auto& a: analysis ){
    delete a;
  }
  analysis.clear();
}

Rule* Mbma::matchRule( const std::vector<std::string>& ana,
		       const UnicodeString& word ){
  Rule *rule = new Rule( ana, word, mbmaLog, debugFlag );
  if ( rule->performEdits() ){
    rule->reduceZeroNodes();
    if ( debugFlag ){
      *Log(mbmaLog) << "after reduction: " << rule << endl;
    }
#ifdef OLD
    rule->resolve_inflections();
    if ( debugFlag ){
      *Log(mbmaLog) << "after resolving: " << rule << endl;
    }
#else
    //    rule->debugFlag = 1;
    if ( !rule->check_inflections() ){
      if ( debugFlag ){
	*Log(mbmaLog) << "failed inflection check: " << rule << endl;
      }
      delete rule;
      return 0;
    }
    //    rule->debugFlag = 0;
#endif
    rule->resolveBrackets( doDaring );
    rule->getCleanInflect();
    if ( debugFlag ){
      *Log(mbmaLog) << "1 added Inflection: " << rule << endl;
    }
    return rule;
  }
  else {
    if ( debugFlag ){
      *Log(mbmaLog) << "rejected rule: " << rule << endl;
    }
    delete rule;
    return 0;
  }
}

vector<Rule*> Mbma::execute( const UnicodeString& word,
			     const vector<string>& classes ){
  vector<vector<string> > allParts = generate_all_perms( classes );
  if ( debugFlag ){
    string out = "alternatives: word=" + UnicodeToUTF8(word) + ", classes=<";
    for ( size_t i=0; i < classes.size(); ++i )
      out += classes[i] + ",";
    out += ">";
    *Log(mbmaLog) << out << endl;
    *Log(mbmaLog) << "allParts : " << allParts << endl;
  }

  vector<Rule*> accepted;
  // now loop over all the analysis
  for ( auto const& ana : allParts ){
    Rule *rule = matchRule( ana, word );
    if ( rule )
      accepted.push_back( rule );
  }
  return accepted;
}

void Mbma::addMorph( Word *word,
		     const vector<string>& morphs ) const {
  KWargs args;
  args["set"] = mbma_tagset;
  MorphologyLayer *ml;
#pragma omp critical(foliaupdate)
  {
    try {
      ml = word->addMorphologyLayer( args );
    }
    catch( const exception& e ){
      *Log(mbmaLog) << e.what() << " addMorph failed." << endl;
      exit(EXIT_FAILURE);
    }
  }
  addMorph( ml, morphs );
}

void Mbma::addBracketMorph( Word *word,
			    const string& wrd,
			    const string& tag ) const {
  //  *Log(mbmaLog) << "addBracketMorph(" << wrd << "," << tag << ")" << endl;
  string celex_tag = tag;
  string head = tag;
  if ( head == "LET" || head == "SPEC" ){
    head.clear();
  }
  else if ( head == "X" ) {
    // unanalysed, so trust the TAGGER
#pragma omp critical(foliaupdate)
    {
      const auto pos = word->annotation<PosAnnotation>( cgn_tagset );
      head = pos->feat("head");
    }
    //    cerr << "head was X, now :" << head << endl;
    const auto tagIt = TAGconv.find( head );
    if ( tagIt == TAGconv.end() ) {
      // this should never happen
      throw ValueError( "unknown head feature '" + head + "'" );
    }
    celex_tag = tagIt->second;
    head = CLEX::get_tDescr(CLEX::toCLEX(tagIt->second));
    //    cerr << "head was X, now :" << head << endl;
  }

  KWargs args;
  args["set"] = mbma_tagset;
  MorphologyLayer *ml;
#pragma omp critical(foliaupdate)
  {
    try {
      ml = word->addMorphologyLayer( args );
    }
    catch( const exception& e ){
      *Log(mbmaLog) << e.what() << " addBracketMorph failed." << endl;
      exit(EXIT_FAILURE);
    }
  }
  args["class"] = "stem";
  Morpheme *result = new Morpheme( word->doc(), args );
  args.clear();
  args["value"] = wrd;
  TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
  {
    result->append( t );
  }
  args.clear();
  args["subset"] = "structure";
  args["class"]  = "[" + wrd + "]" + head;
#pragma omp critical(foliaupdate)
  {
    folia::Feature *feat = new folia::Feature( args );
    result->append( feat );
  }
  args.clear();
  args["set"] = clex_tagset;
  args["cls"] = celex_tag;
#pragma omp critical(foliaupdate)
  {
    result->addPosAnnotation( args );
  }
#pragma omp critical(foliaupdate)
  {
    ml->append( result );
  }
}

void Mbma::addBracketMorph( Word *word,
			    const string& orig_word,
			    const BracketNest *brackets ) const {
  KWargs args;
  args["set"] = mbma_tagset;
  MorphologyLayer *ml;
#pragma omp critical(foliaupdate)
  {
    try {
      ml = word->addMorphologyLayer( args );
    }
    catch( const exception& e ){
      *Log(mbmaLog) << e.what() << " addBracketMorph failed." << endl;
      exit(EXIT_FAILURE);
    }
  }
  Morpheme *m = brackets->createMorpheme( word->doc(),
					  mbma_tagset,
					  clex_tagset );
  if ( m ){
    args.clear();
    args["value"] = orig_word;
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      m->append( t );
      ml->append( m );
    }
  }
}

void Mbma::addMorph( MorphologyLayer *ml,
		     const vector<string>& morphs ) const {
  for ( size_t p=0; p < morphs.size(); ++p ){
    KWargs args;
    args["set"] = mbma_tagset;
    Morpheme *m = new Morpheme( ml->doc(), args );
#pragma omp critical(foliaupdate)
    {
      ml->append( m );
    }
    args.clear();
    args["value"] = morphs[p];
    TextContent *t = new TextContent( args );
#pragma omp critical(foliaupdate)
    {
      m->append( t );
    }
  }
}

bool mbmacmp( Rule *m1, Rule *m2 ){
  return m1->getKey(false).length() > m2->getKey(false).length();
}

void Mbma::filterHeadTag( const string& head ){
  // first we select only the matching heads
  if (debugFlag){
    *Log(mbmaLog) << "filter with head: " << head << endl;
    *Log(mbmaLog) << "filter: analysis is:" << endl;
    int i=0;
    for( const auto& it : analysis ){
      *Log(mbmaLog) << ++i << " - " << it << endl;
    }
  }
  map<string,string>::const_iterator tagIt = TAGconv.find( head );
  if ( tagIt == TAGconv.end() ) {
    // this should never happen
    throw ValueError( "unknown head feature '" + head + "'" );
  }
  string head_tag = tagIt->second;
  if (debugFlag){
    *Log(mbmaLog) << "#matches: CGN:" << head << " CELEX " << head_tag << endl;
  }
  auto ait = analysis.begin();
  while ( ait != analysis.end() ){
    string tagI = CLEX::toString((*ait)->tag);
    if ( ( head_tag == tagI )
	 || ( head_tag == "N" && tagI == "PN" ) ){
      if (debugFlag){
	*Log(mbmaLog) << "comparing " << head_tag << " with "
		      << tagI << " (OK)" << endl;
      }
      ++ait;
    }
    else {
      if (debugFlag){
	*Log(mbmaLog) << "comparing " << head_tag << " with "
		      << tagI << " (rejected)" << endl;
      }
      delete *ait;
      ait = analysis.erase( ait );
    }
  }
  if (debugFlag){
    *Log(mbmaLog) << "filter: analysis after head filter:" << endl;
    int i=0;
    for( const auto& it : analysis ){
      *Log(mbmaLog) << ++i << " - " << it << endl;
    }
  }
}

void Mbma::filterSubTags( const vector<string>& feats ){
  if ( analysis.size() < 1 ){
    if (debugFlag ){
      *Log(mbmaLog) << "analysis is empty so skip next filter" << endl;
    }
    return;
  }
  // ok there are several analysis left.
  // try to select on the features:
  //
  // find best match
  // loop through all subfeatures of the tag
  // and match with inflections from each m
  set<Rule *> bestMatches;
  int max_count = 0;
  for ( auto& q : analysis ){
    int match_count = 0;
    string inflection = q->inflection;
    if ( inflection.empty() ){
      bestMatches.insert(q);
      continue;
    }
    if (debugFlag){
      *Log(mbmaLog) << "matching " << inflection << " with " << feats << endl;
    }
    for ( size_t u=0; u < feats.size(); ++u ) {
      map<string,string>::const_iterator conv_tag_p = TAGconv.find(feats[u]);
      if (conv_tag_p != TAGconv.end()) {
	string c = conv_tag_p->second;
	if (debugFlag){
	  *Log(mbmaLog) << "found " << feats[u] << " ==> " << c << endl;
	}
	if ( inflection.find( c ) != string::npos ){
	  if (debugFlag){
	    *Log(mbmaLog) << "it is in the inflection " << endl;
	  }
	  match_count++;
	}
      }
    }
    if (debugFlag)
      *Log(mbmaLog) << "score: " << match_count << " max was " << max_count << endl;
    if (match_count >= max_count) {
      if (match_count > max_count) {
	max_count = match_count;
	bestMatches.clear();
      }
      bestMatches.insert(q);
    }
  }
  //
  // we still might have doubles. (different Rule's yielding the same result)
  // reduce these
  //
  map<UnicodeString, Rule*> unique;
  for ( const auto& ait : bestMatches ){
    UnicodeString tmp = ait->getKey( doDaring );
    unique[tmp] = ait;
  }
  // so now we have map of 'equal' analysis.
  // create a set for revers lookup.
  set<Rule*> uniqueAna;
  for ( auto const& uit : unique ){
    uniqueAna.insert( uit.second );
  }
  // now we can remove all analysis that aren't in that set.
  auto it = analysis.begin();
  while ( it != analysis.end() ){
    if ( uniqueAna.find( *it ) == uniqueAna.end() ){
      delete *it;
      it = analysis.erase( it );
    }
    else {
      ++it;
    }
  }
  if ( debugFlag ){
    *Log(mbmaLog) << "filter: analysis before sort on length:" << endl;
    int i=0;
    for( const auto& it : analysis ){
      *Log(mbmaLog) << ++i << " - " << it << " " << it->getKey(false)
		    << " (" << it->getKey(false).length() << ")" << endl;
    }
    *Log(mbmaLog) << "" << endl;
  }
  // Now we have a small list of unique and differtent analysis.
  // We assume the 'longest' analysis to be the best.
  // So we prefer '[ge][maak][t]' over '[gemaak][t]'
  // Therefor we sort on (unicode) string length
  sort( analysis.begin(), analysis.end(), mbmacmp );

  if ( debugFlag){
    *Log(mbmaLog) << "filter: definitive analysis:" << endl;
    int i=0;
    for( auto const& it : analysis ){
      *Log(mbmaLog) << ++i << " - " << it << endl;
    }
    *Log(mbmaLog) << "done filtering" << endl;
  }
  return;
}

void Mbma::getFoLiAResult( Word *fword, const UnicodeString& uword ) const {
  if ( analysis.size() == 0 ){
    // fallback option: use the word and pretend it's a morpheme ;-)
    if ( debugFlag ){
      *Log(mbmaLog) << "no matches found, use the word instead: "
		    << uword << endl;
    }
    if ( doDaring ){
      addBracketMorph( fword, UnicodeToUTF8(uword), "X" );
    }
    else {
      vector<string> tmp;
      tmp.push_back( UnicodeToUTF8(uword) );
      addMorph( fword, tmp );
    }
  }
  else {
    for( auto const& sit : analysis ){
      if ( doDaring ){
	addBracketMorph( fword, UnicodeToUTF8(uword), sit->brackets );
      }
      else {
	addMorph( fword, sit->extract_morphemes() );
      }
    }
  }
}

void Mbma::addDeclaration( Document& doc ) const {
#pragma omp critical(foliaupdate)
  {
    doc.declare( AnnotationType::MORPHOLOGICAL, mbma_tagset,
		 "annotator='frog-mbma-" +  version +
		 + "', annotatortype='auto', datetime='" + getTime() + "'");
    if ( doDaring ){
      doc.declare( AnnotationType::POS, clex_tagset,
		   "annotator='frog-mbma-" +  version +
		   + "', annotatortype='auto', datetime='" + getTime() + "'");
    }
  }
}

UnicodeString Mbma::filterDiacritics( const UnicodeString& in ) const {
  if ( transliterator ){
    UnicodeString result = in;
    transliterator->transliterate( result );
    return result;
  }
  else
    return in;
}

void Mbma::Classify( Word* sword ){
  if ( sword->isinstance(PlaceHolder_t) )
    return;
  UnicodeString uWord;
  PosAnnotation *pos;
  string head;
  string token_class;
#pragma omp critical(foliaupdate)
  {
    uWord = sword->text();
    pos = sword->annotation<PosAnnotation>( cgn_tagset );
    head = pos->feat("head");
    token_class = sword->cls();
  }
  if (debugFlag)
    *Log(mbmaLog) << "Classify " << uWord << "(" << pos << ") ["
		  << token_class << "]" << endl;
  if ( filter )
    uWord = filter->filter( uWord );
  if ( head == "LET" || head == "SPEC" ){
    // take over the letter/word 'as-is'.
    string word = UnicodeToUTF8( uWord );
    if ( doDaring ){
      addBracketMorph( sword, word, head );
    }
    else {
      vector<string> tmp;
      tmp.push_back( word );
      addMorph( sword, tmp );
    }
  }
  else {
    UnicodeString lWord = uWord;
    if ( head != "SPEC" )
      lWord.toLower();
    Classify( lWord );
    vector<string> featVals;
#pragma omp critical(foliaupdate)
    {
      vector<folia::Feature*> feats = pos->select<folia::Feature>();
      featVals.reserve( feats.size() );
      for ( const auto& feat : feats )
	featVals.push_back( feat->cls() );
    }
    filterHeadTag( head );
    filterSubTags( featVals );
    getFoLiAResult( sword, lWord );
  }
}

void Mbma::Classify( const UnicodeString& word ){
  clearAnalysis();
  UnicodeString uWord = filterDiacritics( word );
  vector<string> insts = make_instances( uWord );
  vector<string> classes;
  classes.reserve( insts.size() );
  int i = 0;
  for( auto const& inst : insts ) {
    string ans;
    MTree->Classify( inst, ans );
    if ( debugFlag ){
      *Log(mbmaLog) << "itt #" << i+1 << " " << insts[i] << " ==> " << ans
		    << ", depth=" << MTree->matchDepth() << endl;
      ++i;
    }
    classes.push_back( ans);
  }

  // fix for 1st char class ==0
  if ( classes[0] == "0" )
    classes[0] = "X";
  analysis = execute( uWord, classes );
}

vector<vector<string> > Mbma::getResult( bool show_class ) const {
  vector<vector<string> > result;
  result.reserve( analysis.size() );
  if ( doDaring ){
    for ( const auto& it : analysis ){
      stringstream ss;
      ss << it->brackets->put( !show_class ) << endl;
      vector<string> mors;
      mors.push_back( ss.str() );
      if ( debugFlag ){
	*Log(mbmaLog) << "Morphs " << mors << endl;
      }
      result.push_back( mors );
    }
  }
  else {
    for ( const auto& it : analysis ){
      vector<string> mors = it->extract_morphemes();
      if ( debugFlag ){
	*Log(mbmaLog) << "Morphs " << mors << endl;
      }
      result.push_back( mors );
    }
  }
  if ( debugFlag ){
    *Log(mbmaLog) << "result of morph analyses: " << result << endl;
  }
  return result;
}
