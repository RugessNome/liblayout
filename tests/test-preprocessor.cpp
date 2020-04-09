// Copyright (C) 2019 Vincent Chambrin
// This file is part of the typeset project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "test.h"

#include "tex/lexer.h"
#include "tex/parsing/preprocessor.h"
#include "tex/parsing/registers.h"

using namespace tex;

static std::vector<parsing::Token> tokenize(const std::string& text)
{
  parsing::Lexer lex;

  for (char c : text)
    lex.write(c);

  return lex.output();
}

TEST_CASE(test_macro)
{
  using namespace tex;
  using namespace parsing;

  {
    Macro m{ "foo", tokenize("#1"), tokenize("Hello #1!") };

    Macro::MatchResult r = m.match(tokenize("{there}"));

    ASSERT(r.result == Macro::MatchResult::CompleteMatch);
    ASSERT(r.arguments.at(0) == tokenize("there"));

    std::vector<Token> exp = m.expand(r.arguments);

    ASSERT(exp == tokenize("Hello there!"));
  }

  {
    Macro m{ "proclaim", tokenize("#1. #2\\par "), tokenize("Statement: #2") };

    Macro::MatchResult r = m.match(tokenize("Theorem 1. Macros are great\\par "));

    ASSERT(r.result == Macro::MatchResult::CompleteMatch);
    ASSERT(r.arguments.at(0) == tokenize("Theorem 1"));
    ASSERT(r.arguments.at(1) == tokenize("Macros are great"));

    std::vector<Token> exp = m.expand(r.arguments);

    ASSERT(exp == tokenize("Statement: Macros are great"));
  }

}

void write(tex::parsing::Preprocessor& preproc, const std::string& str)
{
  for (const auto& t : tokenize(str))
  {
    preproc.write(t);
  }

  while (!preproc.input().empty())
  {
    preproc.advance();
  }
}

TEST_CASE(test_preprocessor_macro)
{
  using namespace tex;
  using namespace parsing;

  Registers registers;
  Preprocessor preproc{ registers };

  for (const auto& t : tokenize("\\def\\foo{Expanded!}\\foo "))
  {
    preproc.write(t);
  }

  while (!preproc.input().empty())
  {
    preproc.advance();
  }

  ASSERT(preproc.output() == tokenize("Expanded!"));

  preproc.output().clear();

  for (const auto& t : tokenize("\\def\\proclaim #1. #2\\par{Statement: #2}\\proclaim Theorem 1. Macros are great\\par "))
  {
    preproc.write(t);
  }

  while (!preproc.input().empty())
  {
    preproc.advance();
  }

  ASSERT(preproc.output() == tokenize("Statement: Macros are great"));
}

TEST_CASE(test_preprocessor_if)
{
  using namespace tex;
  using namespace parsing;

  Registers registers;
  Preprocessor preproc{ registers };

  registers.br = true;

  write(preproc, "\\ifbr T\\else F\\fi ");
  ASSERT(preproc.output() == tokenize("T"));
  preproc.output().clear();

  registers.br = false;
  write(preproc, "\\ifbr T\\else F\\fi ");
  ASSERT(preproc.output() == tokenize("F"));
  preproc.output().clear();

  write(preproc, "\\ifbr \\ifbr A \\fi \\else F\\fi ");
  ASSERT(preproc.output() == tokenize("F"));
  preproc.output().clear();
}

TEST_CASE(test_preprocessor_csname)
{
  using namespace tex;
  using namespace parsing;

  Registers registers;
  Preprocessor preproc{ registers };

  write(preproc, "\\def\\foo{K}");

  write(preproc, "\\csname foo\\endcsname ");

  ASSERT(preproc.output() == tokenize("K"));
  preproc.output().clear();
}

TEST_CASE(test_preprocessor_expandafter)
{
  using namespace tex;
  using namespace parsing;

  Registers registers;
  Preprocessor preproc{ registers };

  write(preproc, "\\expandafter\\def\\csname 123\\endcsname{H}");
  ASSERT(preproc.find("123") != nullptr);

  write(preproc, "\\csname 123\\endcsname ");
  ASSERT(preproc.output() == tokenize("H"));
  preproc.output().clear();

  write(preproc, "\\def\\a[#1]{#1}");
  ASSERT(preproc.find("a") != nullptr);
  write(preproc, "\\def\\args{[FOO]}");
  ASSERT(preproc.find("args") != nullptr);
  write(preproc, "\\expandafter\\a\\args ");
  ASSERT(preproc.output() == tokenize("FOO"));
  preproc.output().clear();

  write(preproc, "\\def\\foo{F}\\def\\bar{B}\\def\\qux{Q}");
  ASSERT(preproc.find("foo") != nullptr && preproc.find("bar") != nullptr && preproc.find("qux") != nullptr);

  write(preproc, "\\expandafter\\foo\\expandafter\\bar\\qux ");
  ASSERT(preproc.output() == tokenize("FBQ"));
  preproc.output().clear();
}