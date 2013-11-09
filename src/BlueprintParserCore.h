//
//  BlueprintParserCore.h
//  snowcrash
//
//  Created by Zdenek Nemec on 5/4/13.
//  Copyright (c) 2013 Apiary Inc. All rights reserved.
//

#ifndef SNOWCRASH_BLUEPRINTPARSERCORE_H
#define SNOWCRASH_BLUEPRINTPARSERCORE_H

#include <algorithm>
#include <sstream>
#include "ParserCore.h"
#include "SourceAnnotation.h"
#include "MarkdownBlock.h"
#include "BlueprintSection.h"
#include "HTTP.h"
#include "Blueprint.h"
#include "BlueprintUtility.h"
#include "StringUtility.h"
#include "SymbolTable.h"

namespace snowcrash {
    
    /**
     *  \brief Blueprint Parser Options.
     *
     *  Controls blueprint parser behavior.
     */
    enum BlueprintParserOption {
        RenderDescriptionsOption = (1 << 0),    /// < Render Markdown in description.
        RequireBlueprintNameOption = (1 << 1)   /// < Treat missing blueprint name as error
    };
    
    typedef unsigned int BlueprintParserOptions;
    
    /**
     *  \brief Parsing subroutine result
     *
     *  Consists of a parsing result report (first) and
     *  %BlockIterator (second) pointing to the last parsed
     *  markdown block.
     */
    typedef std::pair<Result, BlockIterator> ParseSectionResult;
    
    
    /**
     *  Parser Core Data
     */
    struct BlueprintParserCore {
        BlueprintParserCore(BlueprintParserOptions opts,
                            const SourceData& src,
                            const Blueprint& bp)
        : options(opts), sourceData(src), blueprint(bp) {}
        
        /** Parser Options */
        BlueprintParserOptions options;
        
        /** Symbol Table */
        SymbolTable symbolTable;
        
        /** Source Data */
        const SourceData& sourceData;
        
        /** AST being parsed **/
        const Blueprint& blueprint;
        
    private:
        BlueprintParserCore();
        BlueprintParserCore(const BlueprintParserCore&);
        BlueprintParserCore& operator=(const BlueprintParserCore&);
    };
    
    /**
     *  SectionType Parser prototype.
     */
    template<class T>
    struct SectionParser {
        
        /**
         *  \brief  Parse section's blocks.
         *  \param  section     Actual section being parsed.
         *  \param  cur         Cursor withing the section boundaries.
         *  \param  parser      Parser innstance.
         *  \param  output      AST node parsed.
         *  \return %ParseSectionResult pointing to the last block parsed & including 
         *  any possible source annotiations in the form of results or warnigns.
         */
        static ParseSectionResult ParseSection(const BlueprintSection& section,
                                               const BlockIterator& cur,
                                               BlueprintParserCore& parser,
                                               T& output);
    };
    
    /**
     *  \brief Internal list items classifier prototype.
     *
     *  The classifier might look ahead.
     */
    template <class T>
    SectionType ClassifyInternaListBlock(const BlockIterator& begin,
                                         const BlockIterator& end);
        
    /**
     *  \brief Block Classifier prototype.
     *
     *  The classifier might look ahead.
     */
    template <class T>
    SectionType ClassifyBlock(const BlockIterator& begin,
                              const BlockIterator& end,
                              const SectionType& context);
    
    // Forward declaration of classifier helpers
    extern bool HasResourceGroupSignature(const MarkdownBlock& block);
    extern bool HasActionSignature(const MarkdownBlock& block);
    extern bool HasResourceSignature(const MarkdownBlock& block);
    extern bool HasPayloadSignature(const BlockIterator& begin, const BlockIterator& end);
    extern bool HasPayloadAssetSignature(const BlockIterator& begin, const BlockIterator& end);
    extern bool HasAssetSignature(const BlockIterator& begin, const BlockIterator& end);
    extern bool HasHeaderSignature(const BlockIterator& begin, const BlockIterator& end);
    extern bool HasParametersSignature(const BlockIterator& begin, const BlockIterator& end);
    
    /**
     *  \brief A Markdown block parser.
     *
     *  Iterates over blocks classifying sections and calling relevant %SectionParser P<T>.
     */
    template <class T, class P>
    struct BlockParser : public P {
        
        // Iterate blocks, classify & parse
        static ParseSectionResult Parse(const BlockIterator& begin,
                                        const BlockIterator& end,
                                        const BlueprintSection& parentSection,
                                        BlueprintParserCore& parser,
                                        T& output) {
            Result result;
            SectionType currentSectionType = UndefinedSectionType;
            BlockIterator currentBlock = begin;
            while (currentBlock != end) {
                
                currentSectionType = ClassifyBlock<T>(currentBlock, end, currentSectionType);
                
                // TODO:
                // Bounds were: std::make_pair(begin, end)
                // Changing bounds to start at current block and use parents bounds to judge first block is the way to go
                BlueprintSection currentSection(currentSectionType, std::make_pair(begin, end), parentSection);
                
                ParseSectionResult sectionResult = P::ParseSection(currentSection,
                                                                   currentBlock,
                                                                   parser,
                                                                   output);
                
                result += sectionResult.first;
                if (result.error.code != Error::OK)
                    break;
                
                if (sectionResult.second == currentBlock)
                    break;

                currentBlock = sectionResult.second;

                if (currentSectionType == UndefinedSectionType)
                    break;
            }
            
            return std::make_pair(result, currentBlock);
        }        
    };
    
    /**
     *  \brief  Skip to the end of a section.
     *  \param  begin   Begin of the section inside a block buffer.
     *  \param  end     End of the block buffer.
     *  \param  sectionBegin    A %MarkdownBlockType of the beginning.
     *  \param  sectionEnd      A %MarkdownBlockType of the end.
     *  \return An iterator pointing to the end of the section.
     *
     *  Advances iterator from the begin of a section to the end 
     *  of a section at the same nesting level.
     */
    FORCEINLINE BlockIterator SkipToSectionEnd(const BlockIterator& begin,
                                               const BlockIterator& end,
                                               MarkdownBlockType sectionBegin,
                                               MarkdownBlockType sectionEnd) {
        
        BlockIterator currentBlock = begin;
        if (currentBlock->type == sectionBegin) {
            int level = 1;
            ++currentBlock;
            while (currentBlock != end && level) {
                if (currentBlock->type == sectionBegin)
                    ++level;
                
                if (currentBlock->type == sectionEnd)
                    --level;
                
                if (level)
                    ++currentBlock;
            }
        }
        
        return currentBlock;
    }
    
    /**
     *  \brief  Parse one line of raw `key:value` data.
     *  \param  line    A line to parse.
     *  \param  keyValuePair    The output buffer to place the parsed data into.
     *  \return True on success, false otherwise.
     */
    FORCEINLINE bool KeyValueFromLine(const std::string& line,
                                      KeyValuePair& keyValuePair) {
        
        std::vector<std::string> rawMetadata = SplitOnFirst(line, ':');
        if (rawMetadata.size() != 2)
            return false;
        
        keyValuePair = std::make_pair(rawMetadata[0], rawMetadata[1]);
        TrimString(keyValuePair.first);
        TrimString(keyValuePair.second);
        
        return (!keyValuePair.first.empty() && !keyValuePair.second.empty());
    }
    
    /**
     *  \brief  Construct an Unexpected block error.
     *  \param  block   A Markdown block that is unexpected.
     *  \param  sourceData   Source data byte buffer.
     *  \return An Error with description relevant to the type of the unexpected block.
     */
    FORCEINLINE Error UnexpectedBlockError(const MarkdownBlock& block, const SourceData& sourceData) {
        static const char *NotExpectedMessage = " is either not appropriate for the current context or its keyword has not been recognized, ";
        
        std::stringstream ss;
        ss << "unexpected block, ";
        
        switch (block.type) {
            case HeaderBlockType:
                ss << "this Markdown header" << NotExpectedMessage;
                ss << "recognized keywords include: Group, <HTTP method>, <URI template>, Object ...";
                break;
                
            case ListBlockBeginType:
            case ListItemBlockBeginType:
                
                ss << "this Markdown list item"  << NotExpectedMessage;
                ss << "recognized keywords include: Request, Response <HTTP status code>, Headers, Parameters ...";
                break;
                
                
            default:
                ss << "a block of this type is not expected in the current context";
                break;
        }
        
        return Error(ss.str(),
                     BusinessError,
                     MapSourceDataBlock(block.sourceMap, sourceData));
    }
    
    
    /** 
     *  \brief  Retrieves source code character map for a markdown block.
     *  \param  cur         A block to retrieve the characters map for.
     *  \param  fallback    Alternative block if %cur map source does not exists.
     *  \param  bounds      Boundaries of the %cur and %fallback container.
     *  \param  sourceData  Source data to map.
     *  \returns            Character map for given markdown block or its alternative block, 
     *                      if exists. Empty source character map otherwise.
     */
    FORCEINLINE SourceCharactersBlock CharacterMapForBlock(const BlockIterator& cur,
                                                           const BlockIterator& fallback,
                                                           const SectionBounds& bounds,
                                                           const SourceData& sourceData)
    {
        // Try to use cursor's source map
        if (cur != bounds.second &&
            !cur->sourceMap.empty()) {
            return MapSourceDataBlock(cur->sourceMap, sourceData);
        }
        
        // Fallback to alternative (previous) block
        if (fallback != bounds.second &&
            !fallback->sourceMap.empty()) {
            return MapSourceDataBlock(fallback->sourceMap, sourceData);
        }
        
        return SourceCharactersBlock();
    }
    
    /**
     *  \brief Checks cursor validity within its container.
     *  \param section   A section to check against.
     *  \param cur  An iterator to be checked.
     *  \param sourceData   Source data byte buffer.
     *  \param result   Error result output, an error object is added in case of failed check.
     *  \returns True if cursor appears to be valid, false otherwise.
     */
    FORCEINLINE bool CheckCursor(const BlueprintSection& section,
                                 const BlockIterator& cur,
                                 const SourceData& sourceData,
                                 Result& result) {
        if (cur != section.bounds.second)
            return true;
        
        if (!section.hasParent() ||
            section.parent().bounds.first->sourceMap.empty())
            return false;
        
        // ERR: Sanity check
        SourceCharactersBlock sourceBlock = CharacterMapForBlock(section.parent().bounds.first,
                                                                 section.parent().bounds.second,
                                                                 section.parent().bounds,
                                                                 sourceData);
        result.error = Error("unexpected markdown closure",
                             ApplicationError,
                             sourceBlock);
        return false;
    }
}

#endif
