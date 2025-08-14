#!/usr/bin/env bash

header="/*
 * Copyright (c) Zeppelin Bend Pty Ltd (Zepben) 2025 - All Rights Reserved.
 * Unauthorized use, copy, or distribution of this file or its contents, via any medium is strictly prohibited.
 */
"

for f in /outputs/java-lib/*.java; do
    if [ -f $f ]; then
        grep "Copyright" $f > /dev/null
        if [ $? != 0 ]; then
            # License not found; add it
            echo "Fixing $f"
            echo "$header" >> $f.new
            cat $f >> $f.new
            mv $f.new $f
        fi
    fi
done
