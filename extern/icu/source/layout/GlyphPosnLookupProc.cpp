/*
 *
 * (C) Copyright IBM Corp. 1998 - 2004 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "Features.h"
#include "Lookups.h"
#include "ScriptAndLanguage.h"
#include "GlyphDefinitionTables.h"
#include "GlyphPositioningTables.h"
#include "SinglePositioningSubtables.h"
#include "PairPositioningSubtables.h"
#include "CursiveAttachmentSubtables.h"
#include "MarkToBasePosnSubtables.h"
#include "MarkToLigaturePosnSubtables.h"
#include "MarkToMarkPosnSubtables.h"
//#include "ContextualPositioningSubtables.h"
#include "ContextualSubstSubtables.h"
#include "ExtensionSubtables.h"
#include "LookupProcessor.h"
#include "GlyphPosnLookupProc.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

// Aside from the names, the contextual positioning subtables are
// the same as the contextual substitution subtables.
typedef ContextualSubstitutionSubtable ContextualPositioningSubtable;
typedef ChainingContextualSubstitutionSubtable ChainingContextualPositioningSubtable;

GlyphPositioningLookupProcessor::GlyphPositioningLookupProcessor(
        const GlyphPositioningTableHeader *glyphPositioningTableHeader,
        LETag scriptTag, LETag languageTag, const LETag *featureOrder)
    : LookupProcessor(
                      (char *) glyphPositioningTableHeader,
                      SWAPW(glyphPositioningTableHeader->scriptListOffset),
                      SWAPW(glyphPositioningTableHeader->featureListOffset),
                      SWAPW(glyphPositioningTableHeader->lookupListOffset),
                      scriptTag, languageTag, featureOrder)
{
    // anything?
}

GlyphPositioningLookupProcessor::GlyphPositioningLookupProcessor()
{
}

le_uint32 GlyphPositioningLookupProcessor::applySubtable(const LookupSubtable *lookupSubtable, le_uint16 lookupType,
                                                       GlyphIterator *glyphIterator,
                                                       const LEFontInstance *fontInstance) const
{
    le_uint32 delta = 0;

    switch(lookupType)
    {
    case 0:
        break;

    case gpstSingle:
    {
        const SinglePositioningSubtable *subtable = (const SinglePositioningSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

    case gpstPair:
    {
        const PairPositioningSubtable *subtable = (const PairPositioningSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

    case gpstCursive:
    {
        const CursiveAttachmentSubtable *subtable = (const CursiveAttachmentSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

    case gpstMarkToBase:
    {
        const MarkToBasePositioningSubtable *subtable = (const MarkToBasePositioningSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

     case gpstMarkToLigature:
    {
        const MarkToLigaturePositioningSubtable *subtable = (const MarkToLigaturePositioningSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

    case gpstMarkToMark:
    {
        const MarkToMarkPositioningSubtable *subtable = (const MarkToMarkPositioningSubtable *) lookupSubtable;

        delta = subtable->process(glyphIterator, fontInstance);
        break;
    }

   case gpstContext:
    {
        const ContextualPositioningSubtable *subtable = (const ContextualPositioningSubtable *) lookupSubtable;

        delta = subtable->process(this, glyphIterator, fontInstance);
        break;
    }

    case gpstChainedContext:
    {
        const ChainingContextualPositioningSubtable *subtable = (const ChainingContextualPositioningSubtable *) lookupSubtable;

        delta = subtable->process(this, glyphIterator, fontInstance);
        break;
    }

    case gpstExtension:
    {
        const ExtensionSubtable *subtable = (const ExtensionSubtable *) lookupSubtable;

        delta = subtable->process(this, lookupType, glyphIterator, fontInstance);
        break;
    }

    default:
        break;
    }

    return delta;
}

GlyphPositioningLookupProcessor::~GlyphPositioningLookupProcessor()
{
}

U_NAMESPACE_END
