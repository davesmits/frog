/* ex: set tabstop=8 expandtab: */
/*
  Copyright (c) 2006 - 2017
  CLST  - Radboud University
  ILK   - Tilburg University

  This file is part of frog:

  A Tagger-Lemmatizer-Morphological-Analyzer-Dependency-Parser for
  several languages

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
      https://github.com/LanguageMachines/frog/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl

*/

#ifndef NER_TAGGER_MOD_H
#define NER_TAGGER_MOD_H

class NERTagger {
 public:
  explicit NERTagger( TiCC::LogStream * );
  ~NERTagger();
  bool init( const TiCC::Configuration& );
  void Classify( const std::vector<folia::Word *>& );
  void addDeclaration( folia::Document& ) const;
  void addNERTags( const std::vector<folia::Word*>&,
		   const std::vector<std::string>&,
		   const std::vector<double>& );
  std::string getTagset() const { return tagset; };
  std::vector<Tagger::TagResult> tagLine( const std::string& );
  bool read_gazets( const std::string&, const std::string& );
  void create_ner_list( const std::vector<std::string>&,
			std::vector<std::string>& );
  std::string set_eos_mark( const std::string& );
  bool Generate( const std::string& );
 private:
  bool fill_ners( const std::string&, const std::string&, const std::string& );
  MbtAPI *tagger;
  TiCC::LogStream *nerLog;
  int debug;
  std::string version;
  std::string tagset;
  std::string textclass;
  std::vector<std::map<std::string,std::string>> known_ners;
  Tokenizer::UnicodeFilter *filter;
  int max_ner_size;
  NERTagger( const NERTagger& ){}; //inhibit copy
};

#endif // NER_TAGGER_MOD_H
