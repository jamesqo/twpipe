#include "tokenize_model_builder.h"
#include "twpipe/logging.h"

namespace twpipe {

TokenizeModelBuilder::TokenizeModelBuilder(po::variables_map & conf,
                                           const Alphabet & char_map) : char_map(char_map) {
  model_name = conf["tok-model-name"].as<std::string>();

  if (model_name == "bi-gru") {
    model_type = kLinearGRUTokenizeModel;
  } else if (model_name == "bi-lstm") {
    model_type = kLinearLSTMTokenizeModel;
  } else if (model_name == "seg-gru") {
    model_type = kSegmentalGRUTokenizeModel;
  } else if (model_name == "seg-lstm") {
    model_type = kSegmentalLSTMTokenizeModel;
  } else {
    _ERROR << "[tokenize|model_builder] unknow tokenize model: " << model_name;
  }

  char_size = char_map.size();
  char_dim = (conf.count("tok-char-dim") ? conf["tok-char-dim"].as<unsigned>() : 0);
  hidden_dim = (conf.count("tok-hidden-dim") ? conf["tok-hidden-dim"].as<unsigned>() : 0);
  n_layers = (conf.count("tok-n-layer") ? conf["tok-n-layer"].as<unsigned>() : 0);
}

TokenizeModel * TokenizeModelBuilder::build(dynet::ParameterCollection & model) {
  TokenizeModel * engine = nullptr;
  if (model_type == kLinearGRUTokenizeModel) {
    engine = new LinearGRUTokenizeModel(model, char_size, char_dim, hidden_dim, n_layers,
                                        char_map);
  } else if (model_type == kLinearLSTMTokenizeModel) {
    engine = new LinearLSTMTokenizeModel(model, char_size, char_dim, hidden_dim, n_layers,
                                         char_map);
  } else if (model_type == kSegmentalGRUTokenizeModel) {
    BOOST_ASSERT_MSG(false, "SegmentalRNN not implemented.");
    // model = new twpipe::SegmentalRNNTokenizeModel(conf);
  } else if (model_type == kSegmentalLSTMTokenizeModel) {
    BOOST_ASSERT_MSG(false, "SegmentalRNN not implemented.");
    // model = new twpipe::SegmentalRNNTokenizeModel(conf);
  } else {
    _ERROR << "[tokenize|model_builder] Unknown tokenize model: " << model_name;
    exit(1);
  }

  return engine;
}

void TokenizeModelBuilder::to_json() {
  Model::get()->to_json(Model::kTokenizerName, {
    { "name", model_name },
    { "n-chars", boost::lexical_cast<std::string>(char_size) },
    { "char-dim", boost::lexical_cast<std::string>(char_dim) },
    { "hidden-dim", boost::lexical_cast<std::string>(hidden_dim) },
    { "n-layers", boost::lexical_cast<std::string>(n_layers) }
  });
}

TokenizeModel * TokenizeModelBuilder::from_json(dynet::ParameterCollection & model) {
  TokenizeModel * engine = nullptr;

  Model * globals = Model::get();
  model_name = globals->from_json(Model::kTokenizerName, "name");
  unsigned temp_size = 
    boost::lexical_cast<unsigned>(globals->from_json(Model::kTokenizerName, "n-chars"));
  if (char_size == 0) {
    char_size = temp_size;
  } else {
    BOOST_ASSERT_MSG(char_size == temp_size, "[tokenize|model_builder] char-size mismatch!");
  }
  char_dim = 
    boost::lexical_cast<unsigned>(globals->from_json(Model::kTokenizerName, "char-dim"));
  hidden_dim =
    boost::lexical_cast<unsigned>(globals->from_json(Model::kTokenizerName, "hidden-dim"));
  n_layers =
    boost::lexical_cast<unsigned>(globals->from_json(Model::kTokenizerName, "n-layers"));

  if (model_name == "bi-gru") {
    model_type = kLinearGRUTokenizeModel;
    engine = new LinearGRUTokenizeModel(model, char_size, char_dim, hidden_dim, n_layers,
                                        char_map);
  } else if (model_name == "bi-lstm") {
    model_type = kLinearLSTMTokenizeModel;
    engine = new LinearLSTMTokenizeModel(model, char_size, char_dim, hidden_dim, n_layers,
                                         char_map);
  } else if (model_name == "seg-gru") {
    model_type = kSegmentalGRUTokenizeModel;
    BOOST_ASSERT_MSG(false, "SegmentalRNN not implemented.");
  } else if (model_name == "seg-lstm") {
    model_type = kSegmentalLSTMTokenizeModel;
    BOOST_ASSERT_MSG(false, "SegmentalRNN not implemented.");
  } else {
    _ERROR << "[tokenize|model_builder] unknow tokenize model: " << model_name;
    exit(1);
  }

  globals->from_json(Model::kTokenizerName, model);
  return engine;
}
 

}