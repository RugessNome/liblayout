// Copyright (C) 2020 Vincent Chambrin
// This file is part of the 'typeset' project
// For conditions of distribution and use, see copyright notice in LICENSE

#include "mainwindow.h"

#include "fonttreewidget.h"
#include "typeset-engine.h"
#include "renderwidget.h"

#include "tex/lexer.h"
#include "tex/parsing/mathparserfrontend.h"
#include "tex/math/math-typeset.h"

#include <QCheckBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <QFrame>
#include <QGroupBox>
#include <QSplitter>

#include <chrono>

#include <QDebug>

MainWindow::MainWindow()
{
  setWindowTitle("Equation Editor");

  m_engine = std::make_shared<TypesetEngine>();

  QHBoxLayout* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  QSplitter *horizontal_splitter = new QSplitter(Qt::Horizontal);
  
  QSplitter* vertical_splitter = new QSplitter(Qt::Vertical);
  m_renderwidget = new RenderWidget;
  m_textedit = new QPlainTextEdit;
  vertical_splitter->addWidget(m_renderwidget);
  vertical_splitter->addWidget(m_textedit);

  horizontal_splitter->addWidget(vertical_splitter);

  horizontal_splitter->addWidget(createSettingsWidget());

  layout->addWidget(horizontal_splitter);

  connect(m_textedit, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
}

MainWindow::~MainWindow()
{

}

QWidget* MainWindow::createSettingsWidget()
{
  QWidget* w = new QWidget;

  QVBoxLayout* layout = new QVBoxLayout(w);

  {
    QGroupBox* gb = new QGroupBox("Rendering");

    QVBoxLayout* gbl = new QVBoxLayout(gb);

    m_drawchars_checkbox = new QCheckBox("Draw chars");
    m_drawchars_checkbox->setChecked(true);
    gbl->addWidget(m_drawchars_checkbox);

    m_drawcharbox_checkbox = new QCheckBox("Draw char boxes");
    m_drawcharbox_checkbox->setChecked(false);
    gbl->addWidget(m_drawcharbox_checkbox);

    m_drawlistbox_checkbox = new QCheckBox("Draw list boxes");
    m_drawlistbox_checkbox->setChecked(false);
    gbl->addWidget(m_drawlistbox_checkbox);

    layout->addWidget(gb);
  }

  {
    QGroupBox* gb = new QGroupBox("Font parameters");

    QVBoxLayout* gbl = new QVBoxLayout(gb);

    m_showonlyused_checkbox = new QCheckBox("Show only used");
    m_showonlyused_checkbox->setChecked(false);
    gbl->addWidget(m_showonlyused_checkbox);

    m_font_treewidget = new FontTreeWidget(m_engine);
    gbl->addWidget(m_font_treewidget);

    layout->addWidget(gb);
  }

  layout->addStretch();

  m_report_widget = new QLabel;
  layout->addWidget(m_report_widget);

  connect(m_drawchars_checkbox, &QCheckBox::toggled, this, &MainWindow::onDrawCharChanged);
  connect(m_drawcharbox_checkbox, &QCheckBox::toggled, this, &MainWindow::onDrawCharBoxesChanged);
  connect(m_drawlistbox_checkbox, &QCheckBox::toggled, this, &MainWindow::onDrawListBoxChanged);
  connect(m_showonlyused_checkbox, &QCheckBox::toggled, this, &MainWindow::onShowOnlyUsedFontDimenChanged);

  return w;
}

void MainWindow::onTextChanged()
{
  try 
  {
    processText();
  }
  catch (const std::runtime_error & ex)
  {
    qDebug() << ex.what();
  }
}

void MainWindow::onShowOnlyUsedFontDimenChanged()
{
  m_font_treewidget->showOnlyUsedFontDimen(m_showonlyused_checkbox->isChecked());
}

void MainWindow::onDrawCharChanged()
{
  m_renderwidget->setDrawChars(m_drawchars_checkbox->isChecked());
}

void MainWindow::onDrawCharBoxesChanged()
{
  m_renderwidget->setDrawCharBoxes(m_drawcharbox_checkbox->isChecked());
}

void MainWindow::onDrawListBoxChanged()
{
  m_renderwidget->setDrawListBox(m_drawlistbox_checkbox->isChecked());
}

static double duration_msec(std::chrono::duration<double> diff)
{
  return diff.count() * 1000;
}

void MainWindow::processText()
{
  m_engine->reset();

  std::string text = m_textedit->toPlainText().toStdString();

  auto start = std::chrono::high_resolution_clock::now();

  tex::parsing::Lexer lexer;

  for (char c : text)
  {
    lexer.write(c);
  }

  auto tokenization_end = std::chrono::high_resolution_clock::now();

  tex::parsing::MathParserFrontend parser;

  for (const tex::parsing::Token& tok : lexer.output())
  {
    if (tok.isCharacterToken())
    {
      tex::parsing::CharacterToken ctok = tok.characterToken();

      switch (ctok.category)
      {
      case tex::parsing::CharCategory::GroupBegin:
        parser.beginMathList();
        break;
      case tex::parsing::CharCategory::GroupEnd:
        parser.endMathList();
        break;
      case tex::parsing::CharCategory::Superscript:
        parser.beginSuperscript();
        break;
      case tex::parsing::CharCategory::Subscript:
        parser.beginSubscript();
        break;
      case tex::parsing::CharCategory::Letter:
      case tex::parsing::CharCategory::Other:
        parser.writeChar(ctok.value);
        break;
      default:
        break;
      }
    }
    else if (tok.isControlSequence())
    {
      parser.writeControlSequence(tok.controlSequence());
    }
  }

  parser.finish();

  auto mathparsing_end = std::chrono::high_resolution_clock::now();

  tex::MathTypesetter mathtypesetter{m_engine};
  mathtypesetter.setFonts(m_engine->mathfonts());

  auto hlist = mathtypesetter.mlist2hlist(parser.output());
  auto box = tex::hbox(std::move(hlist));

  auto typesetting_end = std::chrono::high_resolution_clock::now();

  m_renderwidget->setBox(box);

  QString report = 
    "Tokenization: " + QString::number(duration_msec(tokenization_end - start)) + "\n"
    + "Parsing: " + QString::number(duration_msec(mathparsing_end - tokenization_end)) + "\n"
    + "Typesetting: " + QString::number(duration_msec(typesetting_end -mathparsing_end)) + "\n"
    + "Total: " + QString::number(duration_msec(typesetting_end - start)) + "\n";
  m_report_widget->setText(report);

  m_font_treewidget->sync();
}
