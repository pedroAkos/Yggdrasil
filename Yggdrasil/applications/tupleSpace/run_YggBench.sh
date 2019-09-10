#!/usr/bin/env bash

fab f_execute_program:YggTs,YggTsBench${1},${2}

tail -f Results/YggTs-YggTsBench${1}${2}.out