#!/bin/bash

MODEL_FBS_HEADER="tools/model_fbs.h"
MODEL_CODE_HEADER="include/model_generated.h"
MODEL_FBS="model/model.fbs"

function generate_model_fbs()
{
echo "const char * schema_text =" > $MODEL_FBS_HEADER
while read line; do
    echo "\"$line\\n\"">> $MODEL_FBS_HEADER
done < $MODEL_FBS
echo "\"\";" >> $MODEL_FBS_HEADER
}

function generate_model_code()
{
./model/flatc --cpp --gen-mutable --gen-object-api $MODEL_FBS
mv model_generated.h include/
}

generate_model_fbs
generate_model_code


