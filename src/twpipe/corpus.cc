#include "corpus.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include "logging.h"
#include <boost/assert.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

namespace twpipe {

const char* Corpus::UNK = "_UNK_";
const char* Corpus::BAD0 = "_BAD0_";
const char* Corpus::ROOT = "_ROOT_";
const char* Corpus::SPACE = " ";
const unsigned Corpus::BAD_HED = 10000;
const unsigned Corpus::BAD_DEL = 10000;

void parse_to_vector(const ParseUnits& parse,
                     std::vector<unsigned>& heads,
                     std::vector<unsigned>& deprels) {
  heads.clear();
  deprels.clear();
  for (unsigned i = 0; i < parse.size(); ++i) {
    if (parse[i].head < Corpus::BAD_HED) {
      heads.push_back(parse[i].head - 1);
    } else {
      heads.push_back(parse[i].head);
    }
    deprels.push_back(parse[i].deprel);
  }
}

void vector_to_parse(const std::vector<unsigned>& heads,
                     const std::vector<unsigned>& deprels,
                     ParseUnits& parse) {
  parse.clear();
  BOOST_ASSERT_MSG(heads.size() == deprels.size(),
                   "In corpus.cc: vector_to_parse, #heads should be equal to #deprels");

  for (unsigned i = 0; i < heads.size(); ++i) {
    ParseUnit parse_unit;
    parse_unit.head = (heads[i] < Corpus::BAD_HED ? heads[i] + 1 : heads[i]);
    parse_unit.deprel = deprels[i];
    parse.push_back(parse_unit);
  }
}

Corpus::Corpus() :
  n_train(0),
  n_devel(0),
  url_regex("https?:\\/\\/(\\S+|www\\.(\\w+\\.)+\\S*)"),
  user_regex("@\\w+"),
  smile_regex("[8:=;]['`\\\\-]?[)d]+|[)d]+['`\\\\-]?[8:=;]"),
  lolface_regex("[8:=;]['`\\\\-]?p+"),
  sadface_regex("[8:=;]['`\\\\-]?\\(+|\\)+['`\\\\-]?[8:=;]"),
  neuralface_regex("[8:=;]['`\\\\-]?[\\/|l*]"),
  number_regex("^[-+]?[.\\d]*[\\d]+[:,.\\d]*$"),
  heart_regex("<3"),
  repeat_regex("([!?.]){2,}+"),
  elong_regex("(\\S*?)(\\w)\\2{2,}") {
}

std::string _repeat(const boost::smatch & what) {
  return what[1].str();
}

std::string _elong(const boost::smatch & what) {
  return what[1].str() + what[2].str();
}

std::string Corpus::normalize(const std::string & word) const {
  std::string ret = word;
  ret = boost::regex_replace(ret, url_regex, "<url>");
  ret = boost::regex_replace(ret, user_regex, "<user>");
  ret = boost::regex_replace(ret, smile_regex, "<smile>");
  ret = boost::regex_replace(ret, lolface_regex, "<lolface>");
  ret = boost::regex_replace(ret, sadface_regex, "<sadface>");
  ret = boost::regex_replace(ret, neuralface_regex, "<neutralface>");
  ret = boost::regex_replace(ret, heart_regex, "<heart>");
  ret = boost::regex_replace(ret, number_regex, "<number>");
  ret = boost::regex_replace(ret, repeat_regex, _repeat, boost::match_default | boost::format_all);
  ret = boost::regex_replace(ret, elong_regex, _elong);
  boost::to_lower(ret);
  return ret;
}

void Corpus::load_training_data(const std::string& filename) {
  _INFO << "[corpus] reading training data from: " << filename;

  word_map.insert(Corpus::BAD0);
  word_map.insert(Corpus::UNK);
  word_map.insert(Corpus::ROOT);

  norm_map.insert(Corpus::BAD0);
  norm_map.insert(Corpus::UNK);
  norm_map.insert(Corpus::ROOT);

  char_map.insert(Corpus::BAD0);
  char_map.insert(Corpus::UNK);
  char_map.insert(Corpus::ROOT);
  char_map.insert(Corpus::SPACE);

  pos_map.insert(Corpus::ROOT);

  std::ifstream in(filename);
  BOOST_ASSERT_MSG(in, "[corpus] failed to open the training file.");

  n_train = 0;
  std::string data = "";
  std::string line;
  while (std::getline(in, line)) {
    boost::algorithm::trim(line);
    if (line.size() == 0) {
      // end for an instance.
      parse_data(data, training_data[n_train], true);
      data = "";
      ++n_train;
    } else {
      data += (line + "\n");
    }
  }
  if (data.size() > 0) {
    parse_data(data, training_data[n_train], true);
    ++n_train;
  }

  _INFO << "[corpus] loaded " << n_train << " training sentences.";
}

void Corpus::load_devel_data(const std::string& filename) {
  _INFO << "[corpus] reading development data from: " << filename;
  BOOST_ASSERT_MSG(word_map.size() > 1,
                   "[corpus] BAD0 and UNK should be inserted before loading devel data.");

  std::ifstream in(filename);
  BOOST_ASSERT_MSG(in, "[corpus] failed to open the devel file.");

  n_devel = 0;
  std::string data = "";
  std::string line;
  while (std::getline(in, line)) {
    boost::algorithm::trim(line);
    if (line.size() == 0) {
      parse_data(data, devel_data[n_devel], false);
      data = "";
      ++n_devel;
    } else {
      data += (line + "\n");
    }
  }
  if (data.size() > 0) {
    parse_data(data, devel_data[n_devel], false);
    ++n_devel;
  }

  _INFO << "[corpus] loaded " << n_devel << " development sentences.";
}

unsigned utf8_len(unsigned char x) {
  if (x < 0x80) return 1;
  else if ((x >> 5) == 0x06) return 2;
  else if ((x >> 4) == 0x0e) return 3;
  else if ((x >> 3) == 0x1e) return 4;
  else if ((x >> 2) == 0x3e) return 5;
  else if ((x >> 1) == 0x7e) return 6;
  else abort();
}

// id form lemma cpos pos feat head deprel phead pdeprel
// 0  1    2     3    4   5    6     7     8     9
void Corpus::parse_data(const std::string& data, Instance & inst, bool train) {
  std::stringstream S(data);
  std::string line;

  inst.input_units.clear();
  inst.parse_units.clear();

  InputUnit input_unit;
  ParseUnit parse_unit;

  // dummy root at first.
  input_unit.wid = word_map.get(ROOT);
  input_unit.nid = norm_map.get(ROOT);
  input_unit.pid = pos_map.get(ROOT);
  input_unit.aux_wid = input_unit.wid;
  input_unit.word = ROOT;
  input_unit.norm_word = ROOT;
  input_unit.lemma = ROOT;
  input_unit.feature = ROOT;
  inst.input_units.push_back(input_unit);

  parse_unit.head = BAD_HED;
  parse_unit.deprel = BAD_DEL;
  inst.parse_units.push_back(parse_unit);

  std::string guessed_raw_sentence = "";
  while (std::getline(S, line)) {
    std::vector<std::string> tokens;
    boost::algorithm::trim(line);
    if (boost::algorithm::starts_with(line, "# text = ")) {
      inst.raw_sentence = line.substr(9);
    } else {
      boost::algorithm::split(tokens, line, boost::is_any_of("\t"), boost::token_compress_on);
      BOOST_ASSERT_MSG(tokens.size() > 6, "[corpus] illegal conllu format, number of column less than 6.");      
      
      if (train) {
        const std::string & word = input_unit.word = tokens[1];
        const std::string & norm = input_unit.norm_word = normalize(word);
        input_unit.lemma = tokens[2];
        input_unit.feature = tokens[5];

        input_unit.wid = word_map.insert(word);
        // norm_map should be fix.
        input_unit.nid = (norm_map.contains(norm) ? norm_map.get(norm) : norm_map.get(Corpus::UNK));
        input_unit.pid = pos_map.insert(tokens[3]);
        input_unit.aux_wid = input_unit.wid;

        unsigned cur = 0;
        input_unit.cids.clear();
        while (cur < word.size()) {
          unsigned len = utf8_len(word[cur]);
          input_unit.cids.push_back(char_map.insert(word.substr(cur, len)));
          cur += len;
        }
        inst.input_units.push_back(input_unit);

        parse_unit.head = boost::lexical_cast<unsigned>(tokens[6]);
        parse_unit.deprel = deprel_map.insert(tokens[7]);
        inst.parse_units.push_back(parse_unit);
      } else {
        const std::string & word = input_unit.word = tokens[1];
        const std::string & norm = input_unit.norm_word = normalize(word);
        input_unit.lemma = tokens[2];
        input_unit.feature = tokens[5];

        input_unit.wid = (word_map.contains(word) ? word_map.get(word) : word_map.get(UNK));
        input_unit.nid = (norm_map.contains(norm) ? norm_map.get(norm) : norm_map.get(UNK));
        input_unit.pid = pos_map.get(tokens[3]);
        input_unit.aux_wid = input_unit.wid;

        unsigned cur = 0;
        input_unit.cids.clear();
        while (cur < word.size()) {
          unsigned len = utf8_len(word[cur]);
          std::string ch_str = word.substr(cur, len);
          input_unit.cids.push_back(
            char_map.contains(ch_str) ? char_map.get(ch_str) : char_map.get(Corpus::UNK)
          );
          cur += len;
        }
        inst.input_units.push_back(input_unit);

        parse_unit.head = boost::lexical_cast<unsigned>(tokens[6]);
        parse_unit.deprel = deprel_map.insert(tokens[7]);
        inst.parse_units.push_back(parse_unit);
      }
      if (tokens[9] == "SpaceAfter=No" || tokens[9] == "SpaceAfter=\\n") {
        guessed_raw_sentence += tokens[1];
      } else {
        guessed_raw_sentence += (tokens[1] + " ");
      }
    }
  }
  if (inst.raw_sentence == "") {
    inst.raw_sentence = guessed_raw_sentence;
  }
}

unsigned Corpus::get_or_add_word(const std::string& word) {
  return word_map.insert(word);
}

void Corpus::stat() {
  _INFO << "[corpus] # of words = " << word_map.size();
  _INFO << "[corpus] # of norm = " << norm_map.size();
  _INFO << "[corpus] # of char = " << char_map.size();
  _INFO << "[corpus] # of pos = " << pos_map.size();
  _INFO << "[corpus] # of deprel = " << deprel_map.size();
}

void Corpus::get_vocabulary_and_word_count() {
  for (auto& payload : training_data) {
    for (auto& item : payload.second.input_units) {
      training_vocab.insert(item.wid);
      ++counter[item.wid];
    }
  }
}

void load_word_embeddings(const std::string & embedding_file,
                          unsigned pretrained_dim,
                          IntEmbeddingType & pretrained,
                          Alphabet & norm_map) {
  pretrained[norm_map.insert(Corpus::BAD0)] = std::vector<float>(pretrained_dim, 0.);
  pretrained[norm_map.insert(Corpus::UNK)] = std::vector<float>(pretrained_dim, 0.);
  pretrained[norm_map.insert(Corpus::ROOT)] = std::vector<float>(pretrained_dim, 0.);
  _INFO << "[corpus] loading from " << embedding_file << " with " << pretrained_dim << " dimensions.";
  std::ifstream ifs(embedding_file);
  BOOST_ASSERT_MSG(ifs, "Failed to load embedding file.");
  std::string line;
  // get the header in word2vec styled embedding.
  std::getline(ifs, line);
  std::vector<float> v(pretrained_dim, 0.);
  std::string word;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    iss >> word;
    // actually, there should be a checking about the embedding dimension.
    for (unsigned i = 0; i < pretrained_dim; ++i) { iss >> v[i]; }
    unsigned id = norm_map.insert(word);
    pretrained[id] = v;
  }
  _INFO << "[corpus] loaded embedding " << pretrained.size() << " entries.";
}

void load_empty_embeddings(unsigned pretrained_dim,
                           IntEmbeddingType & pretrained,
                           Alphabet & norm_map) {
  pretrained[norm_map.insert(Corpus::BAD0)] = std::vector<float>(pretrained_dim, 0.);
  pretrained[norm_map.insert(Corpus::UNK)] = std::vector<float>(pretrained_dim, 0.);
  pretrained[norm_map.insert(Corpus::ROOT)] = std::vector<float>(pretrained_dim, 0.);
  _INFO << "[corpus] loaded embedding " << pretrained.size() << " entries.";
}

void load_word_embeddings(const std::string & embedding_file,
                          unsigned pretrained_dim,
                          StrEmbeddingType & pretrained) {
  pretrained[Corpus::BAD0] = std::vector<float>(pretrained_dim, 0.);
  pretrained[Corpus::UNK] = std::vector<float>(pretrained_dim, 0.);
  pretrained[Corpus::ROOT] = std::vector<float>(pretrained_dim, 0.);
  _INFO << "[corpus] loading from " << embedding_file << " with " << pretrained_dim << " dimensions.";
  std::ifstream ifs(embedding_file);
  BOOST_ASSERT_MSG(ifs, "Failed to load embedding file.");
  std::string line;
  // get the header in word2vec styled embedding.
  std::getline(ifs, line);
  std::vector<float> v(pretrained_dim, 0.);
  std::string word;
  while (std::getline(ifs, line)) {
    std::istringstream iss(line);
    iss >> word;
    // actually, there should be a checking about the embedding dimension.
    for (unsigned i = 0; i < pretrained_dim; ++i) { iss >> v[i]; }
    pretrained[word] = v;
  }
  _INFO << "[corpus] loaded embedding " << pretrained.size() << " entries.";
}

void load_empty_embeddings(unsigned pretrained_dim,
                           StrEmbeddingType & pretrained) {
  pretrained[Corpus::BAD0] = std::vector<float>(pretrained_dim, 0.);
  pretrained[Corpus::UNK] = std::vector<float>(pretrained_dim, 0.);
  pretrained[Corpus::ROOT] = std::vector<float>(pretrained_dim, 0.);
  _INFO << "[corpus] loaded embedding " << pretrained.size() << " entries.";
}


}