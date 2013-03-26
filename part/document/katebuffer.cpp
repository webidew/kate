/* This file is part of the KDE libraries
   Copyright (c) 2000 Waldo Bastian <bastian@kde.org>
   Copyright (C) 2002-2004 Christoph Cullmann <cullmann@kde.org>
   Copyright (C) 2007 Mirko Stocker <me@misto.ch>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "katebuffer.h"
#include "katebuffer.moc"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "katedocument.h"
#include "katehighlight.h"
#include "kateconfig.h"
#include "kateglobal.h"
#include "kateautoindent.h"

#include <kdebug.h>
#include <kglobal.h>
#include <kcharsets.h>
#include <kde_file.h>

// on the fly compression
#include <kfilterdev.h>
#include <kmimetype.h>

#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>
#include <QtCore/QTextCodec>
#include <QtCore/QDate>

#include <limits.h>

/**
 * Initial value for m_maxDynamicContexts
 */
static const int KATE_MAX_DYNAMIC_CONTEXTS = 512;

/**
 * Create an empty buffer. (with one block with one empty line)
 */
KateBuffer::KateBuffer(KateDocument *doc)
 : Kate::TextBuffer (doc),
   m_doc (doc),
   m_brokenEncoding (false),
   m_tooLongLinesWrapped (false),
   m_highlight (0),
   m_tabWidth (8),
   m_lineHighlighted (0),
   m_maxDynamicContexts (KATE_MAX_DYNAMIC_CONTEXTS)
{
  // we need kate global to stay alive
  KateGlobal::incRef ();
}

/**
 * Cleanup on destruction
 */
KateBuffer::~KateBuffer()
{
  // release HL
  if (m_highlight)
    m_highlight->release();

  // release kate global
  KateGlobal::decRef ();
}

void KateBuffer::editStart ()
{
  if (!startEditing ())
    return;
}

void KateBuffer::editEnd ()
{
  /**
   * not finished, do nothing
   */
  if (!finishEditing())
    return;

  /**
   * nothing change, OK
   */
  if (!editingChangedBuffer ())
    return;

  /**
   * if we arrive here, line changed should be OK
   */
  Q_ASSERT (editingMinimalLineChanged () != -1);
  Q_ASSERT (editingMaximalLineChanged () != -1);
  Q_ASSERT (editingMinimalLineChanged () <= editingMaximalLineChanged ());
  
  /**
   * no highlighting, nothing to do
   */
  if (!m_highlight)
    return;

  /**
   * if we don't touch the highlighted area => fine
   */
  if (editingMinimalLineChanged() > m_lineHighlighted)
    return;

  /**
   * look one line too far, needed for linecontinue stuff
   */
  int editTagLineEnd = editingMaximalLineChanged () + 1;
  int editTagLineStart = editingMinimalLineChanged ();

  /**
   * look one line before, needed nearly 100% only for indentation based folding !
   */
  if (editTagLineStart > 0)
    --editTagLineStart;

  /**
   * really update highlighting
   */
  doHighlight (
      editTagLineStart,
      editTagLineEnd,
      true);
}

void KateBuffer::clear()
{
  // call original clear function
  Kate::TextBuffer::clear ();

  // reset the state
  m_brokenEncoding = false;
  m_tooLongLinesWrapped = false;

  // back to line 0 with hl
  m_lineHighlighted = 0;
}

bool KateBuffer::openFile (const QString &m_file, bool enforceTextCodec)
{
  // first: setup fallback and normal encoding
  setEncodingProberType (KateGlobalConfig::global()->proberType ());
  setFallbackTextCodec (KateGlobalConfig::global()->fallbackCodec ());
  setTextCodec (m_doc->config()->codec ());

  // setup eol
  setEndOfLineMode ((EndOfLineMode) m_doc->config()->eol());

  // NOTE: we do not remove trailing spaces on load. This was discussed
  //       over the years again and again. bugs: 306926, 239077, ...

  // line length limit
  setLineLengthLimit (m_doc->config()->lineLengthLimit());

  // then, try to load the file
  m_brokenEncoding = false;
  m_tooLongLinesWrapped = false;
  if (!load (m_file, m_brokenEncoding, m_tooLongLinesWrapped, enforceTextCodec))
    return false;

  // save back encoding
  m_doc->config()->setEncoding (textCodec()->name());

  // set eol mode, if a eol char was found
  if (m_doc->config()->allowEolDetection())
    m_doc->config()->setEol (endOfLineMode ());

  // generate a bom?
  if (generateByteOrderMark())
    m_doc->config()->setBom (true);

  // okay, loading did work
  return true;
}

bool KateBuffer::canEncode ()
{
  QTextCodec *codec = m_doc->config()->codec();

  kDebug(13020) << "ENC NAME: " << codec->name();

  // hardcode some unicode encodings which can encode all chars
  if ((QString(codec->name()) == "UTF-8") || (QString(codec->name()) == "ISO-10646-UCS-2"))
    return true;

  for (int i=0; i < lines(); i++)
  {
    if (!codec->canEncode (line(i)->string()))
    {
      kDebug(13020) << "STRING LINE: " << line(i)->string();
      kDebug(13020) << "ENC WORKING: FALSE";

      return false;
    }
  }

  return true;
}

bool KateBuffer::saveFile (const QString &m_file)
{
  // first: setup fallback and normal encoding
  setEncodingProberType (KateGlobalConfig::global()->proberType ());
  setFallbackTextCodec (KateGlobalConfig::global()->fallbackCodec ());
  setTextCodec (m_doc->config()->codec ());

  // setup eol
  setEndOfLineMode ((EndOfLineMode) m_doc->config()->eol());

  // generate bom?
  setGenerateByteOrderMark (m_doc->config()->bom());

  // append a newline character at the end of the file (eof) ?
  setNewLineAtEof (m_doc->config()->newLineAtEof());

  // try to save
  if (!save (m_file))
    return false;

  // no longer broken encoding, or we don't care
  m_brokenEncoding = false;
  m_tooLongLinesWrapped = false;

  // okay
  return true;
}

void KateBuffer::ensureHighlighted (int line, int lookAhead)
{
  // valid line at all?
  if (line < 0 || line >= lines ())
    return;

  // already hl up-to-date for this line?
  if (line < m_lineHighlighted)
    return;

  // update hl until this line + max lookAhead
  int end = qMin(line + lookAhead, lines ()-1);

  // ensure we have enough highlighted
  doHighlight ( m_lineHighlighted, end, false );
}

void KateBuffer::wrapLine (const KTextEditor::Cursor &position)
{
  // call original
  Kate::TextBuffer::wrapLine (position);

  if (m_lineHighlighted > position.line()+1)
    m_lineHighlighted++;
}

void KateBuffer::unwrapLines (int from, int to)
{
  // catch out of range access, should never happen
  Q_ASSERT(from >= 0);
  Q_ASSERT(to + 1 <= lines());

  for (int line = to; line >= from; --line) {
      if (line + 1 < lines()) {
          Kate::TextBuffer::unwrapLine (line + 1);
        
          if (m_lineHighlighted > (line + 1))
            --m_lineHighlighted;
      }

      // Line "0" can't be unwraped
      // This call is used to unwrap the last line (if last line != 0)
      // This call was used in the previous version too and it looks like the last
      // line can't be unwraped without it
      else if (line) {
          Kate::TextBuffer::unwrapLine (line);
        
          if (m_lineHighlighted > line)
            --m_lineHighlighted;
      }
  }
}

void KateBuffer::unwrapLine (int line)
{
  // reimplemented, so first call original
  Kate::TextBuffer::unwrapLine (line);

  if (m_lineHighlighted > line)
    --m_lineHighlighted;
}

void KateBuffer::setTabWidth (int w)
{
  if ((m_tabWidth != w) && (m_tabWidth > 0))
  {
    m_tabWidth = w;

    if (m_highlight && m_highlight->foldingIndentationSensitive())
      invalidateHighlighting();
  }
}

void KateBuffer::setHighlight(int hlMode)
{
  KateHighlighting *h = KateHlManager::self()->getHl(hlMode);

   // aha, hl will change
  if (h != m_highlight)
  {
    bool invalidate = !h->noHighlighting();

    if (m_highlight)
    {
      m_highlight->release();
      invalidate = true;
    }

    h->use();

    m_highlight = h;

    if (invalidate)
      invalidateHighlighting();

    // inform the document that the hl was really changed
    // needed to update attributes and more ;)
    m_doc->bufferHlChanged ();

    // try to set indentation
    if (!h->indentation().isEmpty())
      m_doc->config()->setIndentationMode (h->indentation());
  }
}

void KateBuffer::invalidateHighlighting()
{
  m_lineHighlighted = 0;
}

void KateBuffer::doHighlight (int startLine, int endLine, bool invalidate)
{
  // no hl around, no stuff to do
  if (!m_highlight)
    return;

#ifdef BUFFER_DEBUGGING
  QTime t;
  t.start();
  kDebug (13020) << "HIGHLIGHTED START --- NEED HL, LINESTART: " << startLine << " LINEEND: " << endLine;
  kDebug (13020) << "HL UNTIL LINE: " << m_lineHighlighted;
  kDebug (13020) << "HL DYN COUNT: " << KateHlManager::self()->countDynamicCtxs() << " MAX: " << m_maxDynamicContexts;
#endif

  // see if there are too many dynamic contexts; if yes, invalidate HL of all documents
  if (KateHlManager::self()->countDynamicCtxs() >= m_maxDynamicContexts)
  {
    {
      if (KateHlManager::self()->resetDynamicCtxs())
      {
#ifdef BUFFER_DEBUGGING
        kDebug (13020) << "HL invalidated - too many dynamic contexts ( >= " << m_maxDynamicContexts << ")";
#endif

        // avoid recursive invalidation
        KateHlManager::self()->setForceNoDCReset(true);

        foreach(KateDocument* doc, KateGlobal::self()->kateDocuments())
          doc->makeAttribs();

        // doHighlight *shall* do his work. After invalidation, some highlight has
        // been recalculated, but *maybe not* until endLine ! So we shall force it manually...
        doHighlight ( m_lineHighlighted, endLine, false );
        m_lineHighlighted = endLine;

        KateHlManager::self()->setForceNoDCReset(false);
        return;
      }
      else
      {
        m_maxDynamicContexts *= 2;

#ifdef BUFFER_DEBUGGING
        kDebug (13020) << "New dynamic contexts limit: " << m_maxDynamicContexts;
#endif
      }
    }
  }

  // get previous line, if any
  Kate::TextLine prevLine;

  if (startLine >= 1)
    prevLine = plainLine (startLine-1);
  else
    prevLine = Kate::TextLine (new Kate::TextLineData ());

  // here we are atm, start at start line in the block
  int current_line = startLine;
  int start_spellchecking = -1;
  int last_line_spellchecking = -1;
  bool ctxChanged = false;
  // loop over the lines of the block, from startline to endline or end of block
  // if stillcontinue forces us to do so
  for (; current_line < qMin (endLine+1, lines()); ++current_line)
  {
    // current line
    Kate::TextLine textLine = plainLine (current_line);

    ctxChanged = false;
    m_highlight->doHighlight (prevLine.data(), textLine.data(), ctxChanged);

#ifdef BUFFER_DEBUGGING
    // debug stuff
    kDebug( 13020 ) << "current line to hl: " << current_line;
    kDebug( 13020 ) << "text length: " << textLine->length() << " attribute list size: " << textLine->attributesList().size();

    const QVector<int> &ml (textLine->attributesList());
    for (int i=2; i < ml.size(); i+=3)
    {
      kDebug( 13020 ) << "start: " << ml.at(i-2) << " len: " << ml.at(i-1) << " at: " << ml.at(i) << " ";
    }
    kDebug( 13020 );
#endif
    
    // need we to continue ?
    bool stillcontinue =  ctxChanged;
    if (stillcontinue && start_spellchecking < 0) {
      start_spellchecking=current_line;
    }
    else if (!stillcontinue && start_spellchecking >= 0) {
      last_line_spellchecking=current_line;
    }

    // move around the lines
    prevLine = textLine;
  }

  /**
   * perhaps we need to adjust the maximal highlighed line
   */
  int oldHighlighted = m_lineHighlighted;
  if (ctxChanged || current_line > m_lineHighlighted)
    m_lineHighlighted = current_line;

  // tag the changed lines !
  if (invalidate) {
#ifdef BUFFER_DEBUGGING
    kDebug (13020) << "HIGHLIGHTED TAG LINES: " << startLine <<  current_line;
#endif

    emit tagLines (startLine, qMax (current_line, oldHighlighted));

    if(start_spellchecking >= 0 && lines() > 0) {
      emit respellCheckBlock(start_spellchecking,
                             qMin(lines()-1, (last_line_spellchecking==-1)?qMax (current_line, oldHighlighted):last_line_spellchecking));
    }
  }

#ifdef BUFFER_DEBUGGING
  kDebug (13020) << "HIGHLIGHTED END --- NEED HL, LINESTART: " << startLine << " LINEEND: " << endLine;
  kDebug (13020) << "HL UNTIL LINE: " << m_lineHighlighted;
  kDebug (13020) << "HL DYN COUNT: " << KateHlManager::self()->countDynamicCtxs() << " MAX: " << m_maxDynamicContexts;
  kDebug (13020) << "TIME TAKEN: " << t.elapsed();
#endif
}

KTextEditor::Range KateBuffer::computeFoldingRangeForStartLine (int startLine)
{
  /**
   * ensure valid input
   */
  Q_ASSERT (startLine >= 0);
  Q_ASSERT (startLine < lines());
  
  /**
   * no highlighting, no folding, ATM
   */
  if (!m_highlight || m_highlight->noHighlighting())
    return KTextEditor::Range::invalid();
  
  /**
   * first: get the wanted start line highlighted
   */
  ensureHighlighted (startLine);
  Kate::TextLine startTextLine = plainLine (startLine);
  
  /**
   * now: decided if indentation based folding or not!
   * FIXME: do this for line context, not globally!
   */
  if (m_highlight->foldingIndentationSensitive()) {
    
    
#if 0
//FIXME FOLDING
bool KateBuffer::isEmptyLine(Kate::TextLine textline)
{
  QLinkedList<QRegExp> l;
  l=m_highlight->emptyLines(textline->attribute(0));
  if (l.isEmpty()) return false;
  QString txt=textline->string();
  foreach(const QRegExp &re,l) {
    if (re.exactMatch(txt)) return true;
  }
  return false;
}
#endif

    
    /**
     * be done now
     */
    return KTextEditor::Range::invalid();
  }
  
  /**
   * 'normal' folding, aka token based like '{' BLUB '}'
   */
  
  /**
   * first step: search the first region type, that stays open for the start line
   */
  short openedRegionType = 0;
  int openedRegionOffset = -1;
  {
    /**
     * mapping of type to "first" offset of it and current number of not matched openings
     */
    QHash<short, QPair<int, int> > foldingStartToOffsetAndCount;
    
    /**
     * walk over all attributes of the line and compute the matchings
     */
    const QVector<Kate::TextLineData::Attribute> &startLineAttributes = startTextLine->attributesList();
    for ( int i = 0; i < startLineAttributes.size(); ++i ) {
      /**
       * folding close?
       */
      if (startLineAttributes[i].foldingValue < 0) {
        /**
         * search for this type, try to decrement counter, perhaps erase element!
         */
        QHash<short, QPair<int, int> >::iterator end = foldingStartToOffsetAndCount.find (-startLineAttributes[i].foldingValue);
        if (end != foldingStartToOffsetAndCount.end()) {
          if (end.value().second > 1)
            --(end.value().second);
          else
            foldingStartToOffsetAndCount.erase (end);
        }
      }
      
      /**
       * folding open?
       */
      if (startLineAttributes[i].foldingValue > 0) {
        /**
         * search for this type, either insert it, with current offset or increment counter!
         */
        QHash<short, QPair<int, int> >::iterator start = foldingStartToOffsetAndCount.find (startLineAttributes[i].foldingValue);
        if (start != foldingStartToOffsetAndCount.end())
          ++(start.value().second);
        else
          foldingStartToOffsetAndCount.insert (startLineAttributes[i].foldingValue, qMakePair (startLineAttributes[i].offset, 1));
      }
    }
    
    /**
     * compute first type with offset
     */
    QHashIterator<short, QPair<int, int> > hashIt (foldingStartToOffsetAndCount);
    while (hashIt.hasNext()) {
       hashIt.next();
       if (openedRegionOffset == -1 || hashIt.value().first < openedRegionOffset) {
          openedRegionType = hashIt.key();
          openedRegionOffset = hashIt.value().first;
       }
    }
  }
  
  /**
   * no opening region found, bad, nothing to do
   */
  if (openedRegionType == 0)
    return KTextEditor::Range::invalid();

  /**
   * second step: search for matching end region marker!
   */
  int countOfOpenRegions = 1;
  for (int line = startLine + 1; line < lines(); ++line) {
    /**
     * ensure line is highlighted
     */
    ensureHighlighted (line);
    Kate::TextLine textLine = plainLine (line);
    
    /**
     * search for matching end marker
     */
    const QVector<Kate::TextLineData::Attribute> &lineAttributes = textLine->attributesList();
    for (int i = 0; i < lineAttributes.size(); ++i) {
      /**
       * matching folding close?
       */
      if (lineAttributes[i].foldingValue == -openedRegionType) {
        --countOfOpenRegions;
        
        /**
         * end reached?
         * return resulting range!
         */
        if (countOfOpenRegions == 0)
          return KTextEditor::Range (KTextEditor::Cursor (startLine, openedRegionOffset), KTextEditor::Cursor (line, lineAttributes[i].offset));
      }
    
      /**
       * matching folding open?
       */
      if (lineAttributes[i].foldingValue == openedRegionType)
        ++countOfOpenRegions;
    }
  }
  
  /**
   * if we arrive here, no range was found!
   */
  return KTextEditor::Range::invalid();
}

// kate: space-indent on; indent-width 2; replace-tabs on;
