#!/bin/bash
DEST_DIR=../../build/proto
SRC_DIR=../3rd/protobuf

DRY_RUN=false
VERBOSE=false

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --dry-run) DRY_RUN=true ;;
        -v) VERBOSE=true ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
    shift
done

mkdir -p $DEST_DIR

find $SRC_DIR -wholename '*test*.proto' -exec grep -q 'syntax = "proto3"' {} \; -exec cp --backup=numbered {} ../../build/proto/ \;

if [ "$DRY_RUN" = true ] && [ "$VERBOSE" = true ]; then
    echo "--- DRY RUN MODE (No files will be changed) ---"
fi

# Target files matching *.proto.~?~ in the current directory and subdirectories

find $DEST_DIR -type f -name "*.proto.~?~" -print0 | while IFS= read -r -d '' file; do
    
    # Extract directory path, filename, and the backup extension character
    dir=$(dirname "$file")
    base=$(basename "$file")
    
    ext="${base#*.~}"  # Extracts the character after .~ (e.g., '1' from 'file.proto.~1~')
    ext="${ext//\~/}"  # Removes remaining tilde
    
    # Strip the trailing .proto.~?~ from the original filename
    name_clean="${base%.proto.~*~}"
    
    # Construct the new filename: *.?.proto
    new_base="${name_clean}.${ext}.proto"
    new_file="${dir}/${new_base}"
    
    if [ "$DRY_RUN" = true ]; then
        if [ "$VERBOSE" = true ]; then
            echo "[WOULD RENAME] $base -> $new_base"
        fi
    else
        if [ "$VERBOSE" = true ]; then
            echo "Renaming: $base -> $new_base"
        fi
        mv "$file" "$new_file"
    fi
    
done

# ~/w/proto23/build$ protoc -Iproto  -I../test/3rd/protobuf/src --plugin=protoc-gen-proto23=./g++-23/protoc-gen-proto23 --proto23_out=out proto/*.proto
