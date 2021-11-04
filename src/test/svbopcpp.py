"""
Test code for svbop.cpp.

Author: Thomas Mortier
Date: November 2021
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import torch
import torch.nn as nn
import time
from svbop_cpp import SVBOP
from main.py.utils import HLabelEncoder, HLabelTransformer
from sklearn import preprocessing
import numpy as np

def test_svbop_flat(n, d, k):
    # generate a random sample with labels
    y = np.random.randint(0,k,n)
    # now convert to numbers in [0,K-1]
    le = preprocessing.LabelEncoder()
    y = le.fit_transform(y)
    # construct PyTorch tensors
    y = torch.tensor(y)
    X = torch.randn(n,d)
    # create softmax layer
    model = SVBOP(d,k)
    # create optimizer and criterion
    optimizer = torch.optim.SGD(model.parameters(), lr = 0.01, momentum=0.9)
    criterion = nn.CrossEntropyLoss()
    # evaluate and backprop
    start_time = time.time()
    out = model(X)
    loss = criterion(out, y) 
    loss.backward()
    optimizer.step()
    optimizer.zero_grad()
    stop_time = time.time()
    print("Total time elapsed = {0}".format(stop_time-start_time))

def test_svbop_hier(n, d, k):
    # generate a random sample with labels
    classes = np.arange(0, k)
    y = np.random.randint(0, k, n)
    hlt = HLabelTransformer(k=(5,50),sep=";",random_state=2021)
    hle = HLabelEncoder(sep=";")
    hlt = hlt.fit(classes)
    hle = hle.fit(hlt.transform(classes))
    # transform labels to hierarchical labels (for some random hierarchy)
    y_t = hlt.transform(y)
    # now convert to numbers in [0,K-1]
    y_t_e = hle.transform(y_t) 
    # construct PyTorch tensors
    y_t_e_tensor = torch.tensor(y_t_e).to(torch.float32)
    X = torch.randn(n,d)
    # create hierarchical softmax layer
    model = SVBOP(d,k,hle.hstruct_)
    # create optimizer and criterion
    optimizer = torch.optim.SGD(model.parameters(),lr=0.01,momentum=0.9)
    criterion = nn.BCELoss()
    # evaluate and backprop
    start_time = time.time()
    out = model(X,y_t_e)
    loss = criterion(out.view(-1),y_t_e_tensor)
    loss.backward()
    optimizer.step()
    optimizer.zero_grad()
    stop_time = time.time()
    print("Total time elapsed = {0}".format(stop_time-start_time))

if __name__=="__main__":
    print("TEST SVBOP FLAT")
    test_svbop_flat(1,1000,10000)
    print("DONE!")
    print("TEST SVBOP HIER")
    test_svbop_hier(1,1000,10000)
    print("DONE!")
