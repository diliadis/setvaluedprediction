/* 
    Implementation of SVBOP by using the LibTorch C++ frontend.
    
    Author: Thomas Mortier
    Date: November 2021
*/
#include <torch/torch.h>
#include <torch/extension.h>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <random>
#include "svbop.h"

/* Code for SVPNode */

/*
  TODO
*/
void SVPNode::addch(int64_t in_features, std::vector<int64_t> y) {
  // check if leaf or internal node 
  if (this->chn.size() > 0)
  {
    // check if y is a subset of one of the children
    int64_t ind = -1;
    for (int64_t i=0; i<static_cast<int64_t>(this->chn.size()); ++i)
    {
      if (std::includes(this->chn[i]->y.begin(), this->chn[i]->y.end(), y.begin(), y.end()) == 1)
      {
        ind = i;
        break;
      }
    }
    if (ind != -1)
      // subset found, hence, recursively pass to child
      this->chn[ind]->addch(in_features, y);
    else
    {
      // no children for which y is a subset, hence, put in children list
      SVPNode* new_node = new SVPNode();
      new_node->y = y;
      new_node->chn = {};
      new_node->par = this->par;
      this->chn.push_back(new_node);
      unsigned long tot_len_y_chn {0};
      for (auto c : this->chn)
        tot_len_y_chn += c->y.size();
      // check if the current node has all its children
      if (tot_len_y_chn == this->y.size())
      {
        // get string representation of y
        std::stringstream ystr;
        std::copy(y.begin(), y.end(), std::ostream_iterator<int>(ystr, " "));
        this->estimator = this->par->register_module(ystr.str(), torch::nn::Linear(in_features, this->chn.size()));
      }
    }
  }
  else
  { 
    // no children yet, hence, put in children list
    SVPNode* new_node = new SVPNode();
    new_node->y = y;
    new_node->chn = {};
    new_node->par = this->par;
    this->chn.push_back(new_node);
  }
}
    
/*
  TODO
*/
torch::Tensor SVPNode::forward(torch::Tensor input, int64_t y_ind) {
  torch::Tensor prob;
  if (this->chn.size() > 1)
  {
    prob = this->estimator->forward(input);
    prob = torch::nn::functional::softmax(prob, torch::nn::functional::SoftmaxFuncOptions(0));
  }
  return prob[y_ind];
}

/* Code for SVPredictor */

/*
  TODO
*/
SVBOP::SVBOP(int64_t in_features, int64_t num_classes, std::vector<std::vector<int64_t>> hstruct) {
  // first create root node 
  this->root = new SVPNode();
  // check if we need a softmax or h-softmax
  if (hstruct.size() == 0) {
    this->root->estimator = this->register_module("linear", torch::nn::Linear(in_features,num_classes));
    this->root->y = {};
    this->root->chn = {};
    this->root->par = this;
  }
  else
  {
    // construct tree for h-softmax
    this->root->y = hstruct[0];
    this->root->chn = {};
    this->root->par = this;
    for (int64_t i=1; i<static_cast<int64_t>(hstruct.size()); ++i)
      this->root->addch(in_features, hstruct[i]);   
  }
}

torch::Tensor SVBOP::forward(torch::Tensor input, std::vector<std::vector<int64_t>> target) {
  if (this->root->y.size() == 0)
  {
    auto o = this->root->estimator->forward(input);
    o = torch::nn::functional::softmax(o, torch::nn::functional::SoftmaxFuncOptions(1));
    return o;
  }
  else
  {
    std::vector<torch::Tensor> probs;
    // run over each sample in batch
    //at::parallel_for(0, batch_size, 0, [&](int64_t start, int64_t end) {
    //for (int64_t bi=start; bi<end; bi++)
    for (int64_t bi=0;bi<input.size(0);++bi)
    {
      // begin at root
      SVPNode* visit_node = this->root;
      torch::Tensor prob = torch::ones(1);
      for (int64_t yi=0;yi<target[bi].size();++yi)
      {
        //prob = prob*1;
        prob = prob*visit_node->forward(input[bi], target[bi][yi]);
        visit_node = visit_node->chn[target[bi][yi]];
      }
      probs.push_back(prob);
    }
    //});
    return torch::stack(probs);
  }
}

/*
torch::Tensor SVBOP::forward(torch::Tensor input, std::vector<int64_t> target) {
  if (this->root->y.size() == 0)
  {
    auto o = this->root->estimator->forward(input);
    o = torch::nn::functional::softmax(o, torch::nn::functional::SoftmaxFuncOptions(1));
    return o;
  }
  else
  {
    int64_t batch_size {input.size(0)};
    std::vector<torch::Tensor> probs;
    // run over each sample in batch
    //at::parallel_for(0, batch_size, 0, [&](int64_t start, int64_t end) {
    //for (int64_t bi=start; bi<end; bi++)
    for (int64_t bi=0;bi<batch_size;++bi)
    {
      // begin at root
      SVPNode* visit_node = this->root;
      torch::Tensor prob = torch::ones(1);
      while (!visit_node->chn.empty())
      {
          int64_t found_ind {-1};
          for (int64_t i=0; i<static_cast<int64_t>(visit_node->chn.size()); ++i)
          { 
              if (std::count(visit_node->chn[i]->y.begin(), visit_node->chn[i]->y.end(), target[bi]))
              {
                  found_ind = i;
                  break;
              }  
          }
          if (found_ind != -1)
          {
              prob = prob*visit_node->forward(input[bi], found_ind);
              visit_node = visit_node->chn[found_ind];
          }
      }
      probs.push_back(prob);
    }
    //});
    return torch::stack(probs);
  }
}
*/

PYBIND11_MODULE(svbop_cpp, m) {
  using namespace pybind11::literals;
  torch::python::bind_module<SVBOP>(m, "SVBOP")
    .def(py::init<int64_t, int64_t, std::vector<std::vector<int64_t>>>(), "in_features"_a, "num_classes"_a, "hstruct"_a=py::list())
    .def("forward", &SVBOP::forward, "input"_a, "target"_a=py::list());
}
