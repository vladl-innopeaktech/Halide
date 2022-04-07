#!/usr/bin/env python

import sys
from jsonpath import JSONPath
import json
import argparse

files = []
x_expr = None
y_expr = None

parser = argparse.ArgumentParser(
        description='Extract columnated data from a collection of JSON files using XPATH expressions.'
        )
parser.add_argument('-x', '--independent_var'
        , help='XPATH expression for the independent variable'
        , metavar="XPATH"
        , type=str , required=True)
parser.add_argument('-y', '--dependent_var'
        , help='XPATH expression for the dependent variable'
        , metavar="XPATH"
        , type=str , required=True)
parser.add_argument('files'
        , help='Files from which to extract data'
        , type=str 
        , nargs='+')
args = parser.parse_args()

x_expr = args.independent_var
y_expr = args.dependent_var
files = args.files

x_var = []
y_var = []

for file in files:
    data = json.load(open(file, "r"))
    x_data = JSONPath(x_expr).parse(data)
    y_data = JSONPath(y_expr).parse(data)
    if x_data == []:
        print(f'{file} does not contain "{x_expr}"');
        exit(2)
    if y_data == []:
        print(f'{file} does not contain "{y_expr}"');
        exit(2)
    x_var.append(sum(map(float, x_data)))
    y_var.append(sum(map(float, y_data)))

points = list(zip(x_var, y_var))
points.sort()

for x,y in points:
    print(f'{x},{y}')
