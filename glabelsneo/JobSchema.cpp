//  JobSchema.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels Neo, a modified version of gLabels-qt.
//
//  gLabels Neo is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels Neo is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels Neo.  If not, see <http://www.gnu.org/licenses/>.
//

#include "JobSchema.hpp"


namespace glabels::cli
{

        QString jobSchema()
        {
                return QString::fromUtf8( R"JSON({
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://github.com/SchneiderWasTaken/glabels-neo/glabelsneo-job.schema.json",
  "title": "glabelsneo job document",
  "description": "Input of 'glabelsneo print --job'. Either {version, jobs: [...]} or, as a one-job shorthand, a single job object. Relative paths (file, data, output) are resolved against the document's directory.",
  "oneOf": [
    {
      "type": "object",
      "required": ["jobs"],
      "additionalProperties": false,
      "properties": {
        "version": { "const": 1 },
        "jobs": { "type": "array", "minItems": 1, "items": { "$ref": "#/$defs/job" } }
      }
    },
    { "$ref": "#/$defs/job" }
  ],
  "$defs": {
    "scalar": {
      "description": "Field values become text: numbers in their shortest exact form (exact up to 2^53, so send long identifiers as strings), booleans as true/false, null as empty.",
      "type": ["string", "number", "boolean", "null"]
    },
    "fieldMap": {
      "type": "object",
      "propertyNames": { "pattern": "^[^_:}$][^:}$]*$" },
      "additionalProperties": { "$ref": "#/$defs/scalar" }
    },
    "options": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "copies":    { "type": "integer", "minimum": 1 },
        "sheets":    { "type": "integer", "minimum": 1 },
        "first":     { "type": "integer", "minimum": 1 },
        "collate":   { "type": "boolean" },
        "group":     { "type": "boolean" },
        "outlines":  { "type": "boolean" },
        "cropMarks": { "type": "boolean" },
        "reverse":   { "type": "boolean" }
      }
    },
    "label": {
      "type": "object",
      "additionalProperties": false,
      "properties": {
        "fields": { "$ref": "#/$defs/fieldMap" },
        "copies": { "type": "integer", "minimum": 1 }
      }
    },
    "job": {
      "type": "object",
      "additionalProperties": false,
      "description": "Each job needs exactly one of printer or output (they may come from the command line instead). labels and data are mutually exclusive.",
      "properties": {
        "version":    { "const": 1 },
        "id":         { "type": "string", "description": "Echoed back in the results so callers can correlate." },
        "file":       { "type": "string", "description": "The .glabels file; \"-\" reads it from standard input." },
        "printer":    { "type": "string", "description": "Installed printer name, or \"default\"." },
        "output":     { "type": "string", "description": "A PDF to write (\"-\" = standard output), or *.png for images." },
        "options":    { "$ref": "#/$defs/options" },
        "variables":  { "$ref": "#/$defs/fieldMap", "description": "Initial values of user variables." },
        "defaults":   { "$ref": "#/$defs/fieldMap", "description": "Fields every label starts from; the label's own values win." },
        "labels":     { "type": "array", "items": { "$ref": "#/$defs/label" } },
        "data":       { "type": "string", "description": "A records file: json, jsonl, csv, tsv, colon or semicolon." },
        "dataFormat": { "type": "string", "enum": ["json", "jsonl", "ndjson", "csv", "csv-nokeys", "tsv", "tsv-nokeys", "colon", "colon-nokeys", "semicolon", "semicolon-nokeys"] }
      },
      "not": { "anyOf": [ { "required": ["printer", "output"] }, { "required": ["labels", "data"] } ] }
    }
  }
}
)JSON" );
        }

}
