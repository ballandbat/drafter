//
//  MSONPropertyMemberParser.h
//  snowcrash
//
//  Created by Pavan Kumar Sunkara on 11/5/14.
//  Copyright (c) 2014 Apiary Inc. All rights reserved.
//

#ifndef SNOWCRASH_MSONPROPERTYMEMBERPARSER_H
#define SNOWCRASH_MSONPROPERTYMEMBERPARSER_H

#include "SectionParser.h"
#include "MSONUtility.h"
#include "MSONValueMemberParser.h"

using namespace scpl;

namespace snowcrash {

    /**
     * MSON Property Member Section Processor
     */
    template<>
    struct SectionProcessor<mson::PropertyMember> : public SignatureSectionProcessorBase<mson::PropertyMember> {

        static SignatureTraits signatureTraits() {

            SignatureTraits signatureTraits(SignatureTraits::IdentifierTrait |
                                            SignatureTraits::ValuesTrait |
                                            SignatureTraits::AttributesTrait |
                                            SignatureTraits::ContentTrait);

            return signatureTraits;
        }

        static void finalizeSignature(const MarkdownNodeIterator& node,
                                      SectionParserData& pd,
                                      const Signature& signature,
                                      const ParseResultRef<mson::PropertyMember>& out) {

            out.node.description = signature.content;

            mson::parsePropertyName(node, pd, signature.identifier, out.report, out.node.name);

            for (std::vector<mdp::ByteBuffer>::const_iterator it = signature.values.begin();
                 it != signature.values.end();
                 it++) {

                out.node.valueDefinition.values.push_back(mson::parseValue(*it));
            }

            mson::parseTypeDefinition(node, pd, signature.attributes, out.report, out.node.valueDefinition.typeDefinition);

            if (!signature.remainingContent.empty()) {

                mson::TypeSection typeSection(mson::BlockDescriptionTypeSectionType);

                typeSection.content.description = signature.remainingContent;
                out.node.sections.push_back(typeSection);
            }
        }

        static MarkdownNodeIterator processDescription(const MarkdownNodeIterator& node,
                                                       const MarkdownNodes& siblings,
                                                       SectionParserData& pd,
                                                       const ParseResultRef<mson::PropertyMember>& out) {

            if (out.node.sections.empty() ||
                (out.node.sections.size() == 1 &&
                 out.node.sections[0].type == mson::BlockDescriptionTypeSectionType)) {

                if (out.node.sections.empty()) {

                    mson::TypeSection typeSection(mson::BlockDescriptionTypeSectionType);
                    out.node.sections.push_back(typeSection);
                }

                if (!out.node.sections[0].content.description.empty()) {
                    TwoNewLines(out.node.sections[0].content.description);
                }

                mdp::ByteBuffer content = mdp::MapBytesRangeSet(node->sourceMap, pd.sourceData);
                out.node.sections[0].content.description += content;
                    
                return ++MarkdownNodeIterator(node);
            }
            
            return node;
        }

        static MarkdownNodeIterator processNestedSection(const MarkdownNodeIterator&,
                                                         const MarkdownNodes&,
                                                         SectionParserData&,
                                                         const ParseResultRef<mson::PropertyMember>&);

        static bool isDescriptionNode(const MarkdownNodeIterator& node,
                                      SectionType sectionType) {

            if (node->type != mdp::ListItemMarkdownNodeType) {
                return true;
            }

            return SectionProcessorBase<mson::PropertyMember>::isDescriptionNode(node, sectionType);
        }

        static SectionType nestedSectionType(const MarkdownNodeIterator&);
    };
    
    /** MSON Property Member Section Parser */
    typedef SectionParser<mson::PropertyMember, ListSectionAdapter> MSONPropertyMemberParser;
}

#endif
