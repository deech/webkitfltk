#! /usr/bin/perl
#
#   This file is part of the WebKit project
#
#   Copyright (C) 1999 Waldo Bastian (bastian@kde.org)
#   Copyright (C) 2007, 2008, 2012, 2014, 2015 Apple Inc. All rights reserved.
#   Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
#   Copyright (C) 2010 Andras Becsi (abecsi@inf.u-szeged.hu), University of Szeged
#   Copyright (C) 2013 Google Inc. All rights reserved.
#
#   This library is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Library General Public
#   License as published by the Free Software Foundation; either
#   version 2 of the License, or (at your option) any later version.
#
#   This library is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Library General Public License for more details.
#
#   You should have received a copy of the GNU Library General Public License
#   along with this library; see the file COPYING.LIB.  If not, write to
#   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
#   Boston, MA 02110-1301, USA.
use Getopt::Long;
use preprocessor;
use strict;
use warnings;

my $defines;
my $preprocessor;
GetOptions('defines=s' => \$defines,
           'preprocessor=s' => \$preprocessor);

my @NAMES = applyPreprocessor("CSSPropertyNames.in", $defines, $preprocessor);
die "We've reached more than 1024 CSS properties, please make sure to update CSSProperty/StylePropertyMetadata accordingly" if (scalar(@NAMES) > 1024);

my %namesHash;
my @duplicates = ();

my $numPredefinedProperties = 1;
my @names = ();
my %nameIsInherited;
my %propertiesWithStyleBuilderOptions;
my %styleBuilderOptions = (
  AnimationProperty => 1, # Defined in Source/WebCore/css/StyleBuilderConverter.h
  AutoFunctions => 1,
  ConditionalConverter => 1,
  Converter => 1,
  Custom => 1,
  FillLayerProperty => 1,
  FontProperty => 1,
  Getter => 1,
  Initial => 1,
  Longhands => 1,
  NameForMethods => 1,
  NoDefaultColor => 1,
  SVG => 1,
  SkipBuilder => 1,
  Setter => 1,
  VisitedLinkColorSupport => 1,
);
my %nameToId;
my @aliases = ();
foreach (@NAMES) {
  next if (m/(^\s*$)/);
  next if (/^#/);

  # Input may use a different EOL sequence than $/, so avoid chomp.
  $_ =~ s/\s*\[(.+?)\]\r?$//;
  my @options = ();
  if ($1) {
    @options = split(/\s*,\s*/, $1);
  }

  $_ =~ s/[\r\n]+$//g;
  if (exists $namesHash{$_}) {
    push @duplicates, $_;
  } else {
    $namesHash{$_} = 1;
  }
  if ($_ =~ /=/) {
    if (@options) {
        die "Options are specified on an alias $_: ", join(", ", @options) . "\n";
    }
    push @aliases, $_;
  } else {
    $nameIsInherited{$_} = 0;
    $propertiesWithStyleBuilderOptions{$_} = {};
    foreach my $option (@options) {
      my ($optionName, $optionValue) = split(/=/, $option);
      if ($optionName eq "Inherited") {
        $nameIsInherited{$_} = 1;
      } elsif ($styleBuilderOptions{$optionName}) {
        $propertiesWithStyleBuilderOptions{$_}{$optionName} = $optionValue;
      } else {
        die "Unrecognized \"" . $optionName . "\" option for " . $_ . " property.";
      }
    }

    my $id = $_;
    $id =~ s/(^[^-])|-(.)/uc($1||$2)/ge;
    $nameToId{$_} = $id;

    push @names, $_;
  }
}

if (@duplicates > 0) {
    die 'Duplicate CSS property names: ', join(', ', @duplicates) . "\n";
}

open GPERF, ">CSSPropertyNames.gperf" || die "Could not open CSSPropertyNames.gperf for writing";
print GPERF << "EOF";
%{
/* This file is automatically generated from CSSPropertyNames.in by makeprop, do not edit */
#include "config.h"
#include \"CSSProperty.h\"
#include \"CSSPropertyNames.h\"
#include \"HashTools.h\"
#include <string.h>

#include <wtf/ASCIICType.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/WTFString.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored \"-Wunknown-pragmas\"
#pragma clang diagnostic ignored \"-Wdeprecated-register\"
#pragma clang diagnostic ignored \"-Wimplicit-fallthrough\"
#endif

namespace WebCore {
EOF

print GPERF "const char* const propertyNameStrings[numCSSProperties] = {\n";
foreach my $name (@names) {
  print GPERF "    \"$name\",\n";
}
print GPERF "};\n\n";

print GPERF << "EOF";
%}
%struct-type
struct Property;
%omit-struct-type
%language=C++
%readonly-tables
%global-table
%compare-strncmp
%define class-name CSSPropertyNamesHash
%define lookup-function-name findPropertyImpl
%define hash-function-name propery_hash_function
%define word-array-name property_wordlist
%enum
%%
EOF

foreach my $name (@names) {
  print GPERF $name . ", CSSProperty" . $nameToId{$name} . "\n";
}

foreach my $alias (@aliases) {
  $alias =~ /^([^\s]*)[\s]*=[\s]*([^\s]*)/;
  my $name = $1;
  print GPERF $name . ", CSSProperty" . $nameToId{$2} . "\n";
}

print GPERF<< "EOF";
%%
const Property* findProperty(const char* str, unsigned int len)
{
    return CSSPropertyNamesHash::findPropertyImpl(str, len);
}

const char* getPropertyName(CSSPropertyID id)
{
    if (id < firstCSSProperty)
        return 0;
    int index = id - firstCSSProperty;
    if (index >= numCSSProperties)
        return 0;
    return propertyNameStrings[index];
}

const AtomicString& getPropertyNameAtomicString(CSSPropertyID id)
{
    if (id < firstCSSProperty)
        return nullAtom;
    int index = id - firstCSSProperty;
    if (index >= numCSSProperties)
        return nullAtom;

    static AtomicString* propertyStrings = new AtomicString[numCSSProperties]; // Intentionally never destroyed.
    AtomicString& propertyString = propertyStrings[index];
    if (propertyString.isNull()) {
        const char* propertyName = propertyNameStrings[index];
        propertyString = AtomicString(propertyName, strlen(propertyName), AtomicString::ConstructFromLiteral);
    }
    return propertyString;
}

String getPropertyNameString(CSSPropertyID id)
{
    // We share the StringImpl with the AtomicStrings.
    return getPropertyNameAtomicString(id).string();
}

String getJSPropertyName(CSSPropertyID id)
{
    char result[maxCSSPropertyNameLength + 1];
    const char* cssPropertyName = getPropertyName(id);
    const char* propertyNamePointer = cssPropertyName;
    if (!propertyNamePointer)
        return emptyString();

    char* resultPointer = result;
    while (char character = *propertyNamePointer++) {
        if (character == '-') {
            char nextCharacter = *propertyNamePointer++;
            if (!nextCharacter)
                break;
            character = (propertyNamePointer - 2 != cssPropertyName) ? toASCIIUpper(nextCharacter) : nextCharacter;
        }
        *resultPointer++ = character;
    }
    *resultPointer = '\\0';
    return WTF::String(result);
}

static const bool isInheritedPropertyTable[numCSSProperties + $numPredefinedProperties] = {
    false, // CSSPropertyInvalid
EOF

foreach my $name (@names) {
  my $id = $nameToId{$name};
  my $value = $nameIsInherited{$name} ? "true " : "false";
  print GPERF "    $value, // CSSProperty$id\n";
}

print GPERF<< "EOF";
};

bool CSSProperty::isInheritedProperty(CSSPropertyID id)
{
    ASSERT(id >= 0 && id <= lastCSSProperty);
    ASSERT(id != CSSPropertyInvalid);
    return isInheritedPropertyTable[id];
}

} // namespace WebCore

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
EOF

close GPERF;

open HEADER, ">CSSPropertyNames.h" || die "Could not open CSSPropertyNames.h for writing";
print HEADER << "EOF";
/* This file is automatically generated from CSSPropertyNames.in by makeprop, do not edit */

#ifndef CSSPropertyNames_h
#define CSSPropertyNames_h

#include <string.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

namespace WTF {
class AtomicString;
class String;
}

namespace WebCore {

enum CSSPropertyID {
    CSSPropertyInvalid = 0,
EOF

my $first = $numPredefinedProperties;
my $i = $numPredefinedProperties;
my $maxLen = 0;
foreach my $name (@names) {
  print HEADER "    CSSProperty" . $nameToId{$name} . " = " . $i . ",\n";
  $i = $i + 1;
  if (length($name) > $maxLen) {
    $maxLen = length($name);
  }
}
my $num = $i - $first;
my $last = $i - 1;

print HEADER "};\n\n";
print HEADER "const int firstCSSProperty = $first;\n";
print HEADER "const int numCSSProperties = $num;\n";
print HEADER "const int lastCSSProperty = $last;\n";
print HEADER "const size_t maxCSSPropertyNameLength = $maxLen;\n";

print HEADER << "EOF";

const char* getPropertyName(CSSPropertyID);
const WTF::AtomicString& getPropertyNameAtomicString(CSSPropertyID id);
WTF::String getPropertyNameString(CSSPropertyID id);
WTF::String getJSPropertyName(CSSPropertyID);

inline CSSPropertyID convertToCSSPropertyID(int value)
{
    ASSERT((value >= firstCSSProperty && value <= lastCSSProperty) || value == CSSPropertyInvalid);
    return static_cast<CSSPropertyID>(value);
}

} // namespace WebCore

namespace WTF {
template<> struct DefaultHash<WebCore::CSSPropertyID> { typedef IntHash<unsigned> Hash; };
template<> struct HashTraits<WebCore::CSSPropertyID> : GenericHashTraits<WebCore::CSSPropertyID> {
    static const bool emptyValueIsZero = true;
    static const bool needsDestruction = false;
    static void constructDeletedValue(WebCore::CSSPropertyID& slot) { slot = static_cast<WebCore::CSSPropertyID>(WebCore::lastCSSProperty + 1); }
    static bool isDeletedValue(WebCore::CSSPropertyID value) { return value == (WebCore::lastCSSProperty + 1); }
};
}

#endif // CSSPropertyNames_h

EOF

close HEADER;

#
# StyleBuilder.cpp generator.
#

sub getScopeForFunction {
  my $name = shift;
  my $builderFunction = shift;

  return $propertiesWithStyleBuilderOptions{$name}{"Custom"}{$builderFunction} ? "StyleBuilderCustom" : "StyleBuilderFunctions";
}

sub getNameForMethods {
  my $name = shift;

  my $nameForMethods = $nameToId{$name};
  $nameForMethods =~ s/Webkit//g;
  if (exists($propertiesWithStyleBuilderOptions{$name}{"NameForMethods"})) {
    $nameForMethods = $propertiesWithStyleBuilderOptions{$name}{"NameForMethods"};
  }
  return $nameForMethods;
}

sub getAutoGetter {
  my $name = shift;
  my $renderStyle = shift;

  return $renderStyle . "->hasAuto" . getNameForMethods($name) . "()";
}

sub getAutoSetter {
  my $name = shift;
  my $renderStyle = shift;

  return $renderStyle . "->setHasAuto" . getNameForMethods($name) . "()";
}

sub getVisitedLinkSetter {
  my $name = shift;
  my $renderStyle = shift;

  return $renderStyle . "->setVisitedLink" . getNameForMethods($name);
}

sub getClearFunction {
  my $name = shift;

  return "clear" . getNameForMethods($name);
}

sub getEnsureAnimationsOrTransitionsMethod {
  my $name = shift;

  return "ensureAnimations" if $name =~ /animation-/;
  return "ensureTransitions" if $name =~ /transition-/;
  die "Unrecognized animation property name.";
}

sub getAnimationsOrTransitionsMethod {
  my $name = shift;

  return "animations" if $name =~ /animation-/;
  return "transitions" if $name =~ /transition-/;
  die "Unrecognized animation property name.";
}

sub getTestFunction {
  my $name = shift;

  return "is" . getNameForMethods($name) . "Set";
}

sub getAnimationMapfunction {
  my $name = shift;

  return "mapAnimation" . getNameForMethods($name);
}

sub getLayersFunction {
  my $name = shift;

  return "backgroundLayers" if $name =~ /background-/;
  return "maskLayers" if $name =~ /mask-/;
  die "Unrecognized FillLayer property name.";
}

sub getLayersAccessorFunction {
  my $name = shift;

  return "ensureBackgroundLayers" if $name =~ /background-/;
  return "ensureMaskLayers" if $name =~ /mask-/;
  die "Unrecognized FillLayer property name.";
}

sub getFillLayerType {
my $name = shift;

  return "BackgroundFillLayer" if $name =~ /background-/;
  return "MaskFillLayer" if $name =~ /mask-/;
}

sub getFillLayerMapfunction {
  my $name = shift;

  return "mapFill" . getNameForMethods($name);
}


foreach my $name (@names) {
  my $nameForMethods = getNameForMethods($name);
  $nameForMethods =~ s/Webkit//g;
  if (exists($propertiesWithStyleBuilderOptions{$name}{"NameForMethods"})) {
    $nameForMethods = $propertiesWithStyleBuilderOptions{$name}{"NameForMethods"};
  }

  if (!exists($propertiesWithStyleBuilderOptions{$name}{"Getter"})) {
    $propertiesWithStyleBuilderOptions{$name}{"Getter"} = lcfirst($nameForMethods);
  }
  if (!exists($propertiesWithStyleBuilderOptions{$name}{"Setter"})) {
    $propertiesWithStyleBuilderOptions{$name}{"Setter"} = "set" . $nameForMethods;
  }
  if (!exists($propertiesWithStyleBuilderOptions{$name}{"Initial"})) {
    if (exists($propertiesWithStyleBuilderOptions{$name}{"FillLayerProperty"})) {
      $propertiesWithStyleBuilderOptions{$name}{"Initial"} = "initialFill" . $nameForMethods;
    } else {
      $propertiesWithStyleBuilderOptions{$name}{"Initial"} = "initial" . $nameForMethods;
    }
  }
  if (!exists($propertiesWithStyleBuilderOptions{$name}{"Custom"})) {
    $propertiesWithStyleBuilderOptions{$name}{"Custom"} = "";
  } elsif ($propertiesWithStyleBuilderOptions{$name}{"Custom"} eq "All") {
    $propertiesWithStyleBuilderOptions{$name}{"Custom"} = "Initial|Inherit|Value";
  }
  my %customValues = map { $_ => 1 } split(/\|/, $propertiesWithStyleBuilderOptions{$name}{"Custom"});
  $propertiesWithStyleBuilderOptions{$name}{"Custom"} = \%customValues;
}

use constant {
  NOT_FOR_VISITED_LINK => 0,
  FOR_VISITED_LINK => 1,
};

sub colorFromPrimitiveValue {
  my $primitiveValue = shift;
  my $forVisitedLink = @_ ? shift : NOT_FOR_VISITED_LINK;

  return "styleResolver.colorFromPrimitiveValue(" . $primitiveValue . ", /* forVisitedLink */ " . ($forVisitedLink ? "true" : "false") . ")";
}

use constant {
  VALUE_IS_COLOR => 0,
  VALUE_IS_PRIMITIVE => 1,
};

sub generateColorValueSetter {
  my $name = shift;
  my $value = shift;
  my $indent = shift;
  my $valueIsPrimitive = @_ ? shift : VALUE_IS_COLOR;

  my $style = "styleResolver.style()";
  my $setterContent .= $indent . "if (styleResolver.applyPropertyToRegularStyle())\n";
  my $setValue = $style . "->" . $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $color = $valueIsPrimitive ? colorFromPrimitiveValue($value) : $value;
  $setterContent .= $indent . "    " . $setValue . "(" . $color . ");\n";
  $setterContent .= $indent . "if (styleResolver.applyPropertyToVisitedLinkStyle())\n";
  $color = $valueIsPrimitive ? colorFromPrimitiveValue($value, FOR_VISITED_LINK) : $value;
  $setterContent .= $indent . "    " . getVisitedLinkSetter($name, $style) . "(" . $color . ");\n";

  return $setterContent;
}

sub handleCurrentColorValue {
  my $name = shift;
  my $primitiveValue = shift;
  my $indent = shift;

  my $code = $indent . "if (" . $primitiveValue . ".getValueID() == CSSValueCurrentcolor) {\n";
  $code .= $indent . "    applyInherit" . $nameToId{$name} . "(styleResolver);\n";
  $code .= $indent . "    return;\n";
  $code .= $indent . "}\n";
  return $code;
}

sub generateAnimationPropertyInitialValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setterContent = "";
  $setterContent .= $indent . "AnimationList& list = styleResolver.style()->" . getEnsureAnimationsOrTransitionsMethod($name) . "();\n";
  $setterContent .= $indent . "if (list.isEmpty())\n";
  $setterContent .= $indent . "    list.append(Animation::create());\n";
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $initial = $propertiesWithStyleBuilderOptions{$name}{"Initial"};
  $setterContent .= $indent . "list.animation(0)." . $setter . "(Animation::" . $initial . "());\n";
  if ($name eq "-webkit-transition-property") {
    $setterContent .= $indent . "list.animation(0).setAnimationMode(Animation::AnimateAll);\n";
  }
  $setterContent .= $indent . "for (size_t i = 1; i < list.size(); ++i)\n";
  $setterContent .= $indent . "    list.animation(i)." . getClearFunction($name) . "();\n";

  return $setterContent;
}

sub generateAnimationPropertyInheritValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setterContent = "";
  $setterContent .= $indent . "AnimationList& list = styleResolver.style()->" . getEnsureAnimationsOrTransitionsMethod($name) . "();\n";
  $setterContent .= $indent . "const AnimationList* parentList = styleResolver.parentStyle()->" . getAnimationsOrTransitionsMethod($name) . "();\n";
  $setterContent .= $indent . "size_t i = 0, parentSize = parentList ? parentList->size() : 0;\n";
  $setterContent .= $indent . "for ( ; i < parentSize && parentList->animation(i)." . getTestFunction($name) . "(); ++i) {\n";
  $setterContent .= $indent . "    if (list.size() <= i)\n";
  $setterContent .= $indent . "        list.append(Animation::create());\n";
  my $getter = $propertiesWithStyleBuilderOptions{$name}{"Getter"};
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  $setterContent .= $indent . "    list.animation(i)." . $setter . "(parentList->animation(i)." . $getter . "());\n";
  $setterContent .= $indent . "    list.animation(i).setAnimationMode(parentList->animation(i).animationMode());\n";
  $setterContent .= $indent . "}\n";
  $setterContent .= "\n";
  $setterContent .= $indent . "/* Reset any remaining animations to not have the property set. */\n";
  $setterContent .= $indent . "for ( ; i < list.size(); ++i)\n";
  $setterContent .= $indent . "    list.animation(i)." . getClearFunction($name) . "();\n";

  return $setterContent;
}

sub generateAnimationPropertyValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setterContent = "";
  $setterContent .= $indent . "AnimationList& list = styleResolver.style()->" . getEnsureAnimationsOrTransitionsMethod($name) . "();\n";
  $setterContent .= $indent . "size_t childIndex = 0;\n";
  $setterContent .= $indent . "if (is<CSSValueList>(value)) {\n";
  $setterContent .= $indent . "    /* Walk each value and put it into an animation, creating new animations as needed. */\n";
  $setterContent .= $indent . "    for (auto& currentValue : downcast<CSSValueList>(value)) {\n";
  $setterContent .= $indent . "        if (childIndex <= list.size())\n";
  $setterContent .= $indent . "            list.append(Animation::create());\n";
  $setterContent .= $indent . "        styleResolver.styleMap()->" . getAnimationMapfunction($name) . "(list.animation(childIndex), currentValue);\n";
  $setterContent .= $indent . "        ++childIndex;\n";
  $setterContent .= $indent . "    }\n";
  $setterContent .= $indent . "} else {\n";
  $setterContent .= $indent . "    if (list.isEmpty())\n";
  $setterContent .= $indent . "        list.append(Animation::create());\n";
  $setterContent .= $indent . "    styleResolver.styleMap()->" . getAnimationMapfunction($name) . "(list.animation(childIndex), value);\n";
  $setterContent .= $indent . "    childIndex = 1;\n";
  $setterContent .= $indent . "}\n";
  $setterContent .= $indent . "for ( ; childIndex < list.size(); ++childIndex) {\n";
  $setterContent .= $indent . "    /* Reset all remaining animations to not have the property set. */\n";
  $setterContent .= $indent . "    list.animation(childIndex)." . getClearFunction($name) . "();\n";
  $setterContent .= $indent . "}\n";

  return $setterContent;
}

sub generateFillLayerPropertyInitialValueSetter {
  my $name = shift;
  my $indent = shift;

  my $getter = $propertiesWithStyleBuilderOptions{$name}{"Getter"};
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $clearFunction = getClearFunction($name);
  my $testFunction = getTestFunction($name);
  my $initial = "FillLayer::" . $propertiesWithStyleBuilderOptions{$name}{"Initial"} . "(" . getFillLayerType($name) . ")";

  my $setterContent = "";
  $setterContent .= $indent . "// Check for (single-layer) no-op before clearing anything.\n";
  $setterContent .= $indent . "const FillLayer& layers = *styleResolver.style()->" . getLayersFunction($name) . "();\n";
  $setterContent .= $indent . "if (!layers.next() && (!layers." . $testFunction . "() || layers." . $getter . "() == $initial))\n";
  $setterContent .= $indent . "    return;\n";
  $setterContent .= "\n";
  $setterContent .= $indent . "FillLayer* child = &styleResolver.style()->" . getLayersAccessorFunction($name) . "();\n";
  $setterContent .= $indent . "child->" . $setter . "(" . $initial . ");\n";
  $setterContent .= $indent . "for (child = child->next(); child; child = child->next())\n";
  $setterContent .= $indent . "    child->" . $clearFunction . "();\n";

  return $setterContent;
}

sub generateFillLayerPropertyInheritValueSetter {
  my $name = shift;
  my $indent = shift;

  my $getter = $propertiesWithStyleBuilderOptions{$name}{"Getter"};
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $clearFunction = getClearFunction($name);
  my $testFunction = getTestFunction($name);

  my $setterContent = "";
  $setterContent .= $indent . "// Check for no-op before copying anything.\n";
  $setterContent .= $indent . "if (*styleResolver.parentStyle()->" . getLayersFunction($name) ."() == *styleResolver.style()->" . getLayersFunction($name) . "())\n";
  $setterContent .= $indent . "    return;\n";
  $setterContent .= "\n";
  $setterContent .= $indent . "auto* child = &styleResolver.style()->" . getLayersAccessorFunction($name) . "();\n";
  $setterContent .= $indent . "FillLayer* previousChild = nullptr;\n";
  $setterContent .= $indent . "for (auto* parent = styleResolver.parentStyle()->" . getLayersFunction($name) . "(); parent && parent->" . $testFunction . "(); parent = parent->next()) {\n";
  $setterContent .= $indent . "    if (!child) {\n";
  $setterContent .= $indent . "        previousChild->setNext(std::make_unique<FillLayer>(" . getFillLayerType($name) . "));\n";
  $setterContent .= $indent . "        child = previousChild->next();\n";
  $setterContent .= $indent . "    }\n";
  $setterContent .= $indent . "    child->" . $setter . "(parent->" . $getter . "());\n";
  $setterContent .= $indent . "    previousChild = child;\n";
  $setterContent .= $indent . "    child = previousChild->next();\n";
  $setterContent .= $indent . "}\n";
  $setterContent .= $indent . "for (; child; child = child->next())\n";
  $setterContent .= $indent . "    child->" . $clearFunction . "();\n";

  return $setterContent;
}

sub generateFillLayerPropertyValueSetter {
  my $name = shift;
  my $indent = shift;

  my $CSSPropertyId = "CSSProperty" . $nameToId{$name};

  my $setterContent = "";
  $setterContent .= $indent . "FillLayer* child = &styleResolver.style()->" . getLayersAccessorFunction($name) . "();\n";
  $setterContent .= $indent . "FillLayer* previousChild = nullptr;\n";
  $setterContent .= $indent . "if (is<CSSValueList>(value)\n";
  $setterContent .= "#if ENABLE(CSS_IMAGE_SET)\n";
  $setterContent .= $indent . "&& !is<CSSImageSetValue>(value)\n";
  $setterContent .= "#endif\n";
  $setterContent .= $indent . ") {\n";
  $setterContent .= $indent . "    // Walk each value and put it into a layer, creating new layers as needed.\n";
  $setterContent .= $indent . "    for (auto& item : downcast<CSSValueList>(value)) {\n";
  $setterContent .= $indent . "        if (!child) {\n";
  $setterContent .= $indent . "            previousChild->setNext(std::make_unique<FillLayer>(" . getFillLayerType($name) . "));\n";
  $setterContent .= $indent . "            child = previousChild->next();\n";
  $setterContent .= $indent . "        }\n";
  $setterContent .= $indent . "        styleResolver.styleMap()->" . getFillLayerMapfunction($name) . "(" . $CSSPropertyId . ", *child, item);\n";
  $setterContent .= $indent . "        previousChild = child;\n";
  $setterContent .= $indent . "        child = child->next();\n";
  $setterContent .= $indent . "    }\n";
  $setterContent .= $indent . "} else {\n";
  $setterContent .= $indent . "    styleResolver.styleMap()->" . getFillLayerMapfunction($name) . "(" . $CSSPropertyId . ", *child, value);\n";
  $setterContent .= $indent . "    child = child->next();\n";
  $setterContent .= $indent . "}\n";
  $setterContent .= $indent . "for (; child; child = child->next())\n";
  $setterContent .= $indent . "    child->" . getClearFunction($name) . "();\n";

  return $setterContent;
}

sub generateSetValueStatement
{
  my $name = shift;
  my $value = shift;

  my $isSVG = exists $propertiesWithStyleBuilderOptions{$name}{"SVG"};
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  return "styleResolver.style()->" .  ($isSVG ? "accessSVGStyle()." : "") . $setter . "(" . $value . ")";
}

sub generateInitialValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $initial = $propertiesWithStyleBuilderOptions{$name}{"Initial"};
  my $isSVG = exists $propertiesWithStyleBuilderOptions{$name}{"SVG"};
  my $setterContent = "";
  $setterContent .= $indent . "static void applyInitial" . $nameToId{$name} . "(StyleResolver& styleResolver)\n";
  $setterContent .= $indent . "{\n";
  my $style = "styleResolver.style()";
  if (exists $propertiesWithStyleBuilderOptions{$name}{"AutoFunctions"}) {
    $setterContent .= $indent . "    " . getAutoSetter($name, $style) . ";\n";
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"VisitedLinkColorSupport"}) {
      my $initialColor = "RenderStyle::" . $initial . "()";
      $setterContent .= generateColorValueSetter($name, $initialColor, $indent . "    ");
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"AnimationProperty"}) {
    $setterContent .= generateAnimationPropertyInitialValueSetter($name, $indent . "    ");
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FontProperty"}) {
    $setterContent .= $indent . "    FontDescription fontDescription = styleResolver.fontDescription();\n";
    $setterContent .= $indent . "    fontDescription." . $setter . "(FontDescription::" . $initial . "());\n";
    $setterContent .= $indent . "    styleResolver.setFontDescription(fontDescription);\n";
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FillLayerProperty"}) {
    $setterContent .= generateFillLayerPropertyInitialValueSetter($name, $indent . "    ");
  } else {
    my $initialValue = ($isSVG ? "SVGRenderStyle" : "RenderStyle") . "::" . $initial . "()";
    $setterContent .= $indent . "    " . generateSetValueStatement($name, $initialValue) . ";\n";
  }
  $setterContent .= $indent . "}\n";

  return $setterContent;
}

sub generateInheritValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setterContent = "";
  $setterContent .= $indent . "static void applyInherit" . $nameToId{$name} . "(StyleResolver& styleResolver)\n";
  $setterContent .= $indent . "{\n";
  my $isSVG = exists $propertiesWithStyleBuilderOptions{$name}{"SVG"};
  my $parentStyle = "styleResolver.parentStyle()";
  my $style = "styleResolver.style()";
  my $getter = $propertiesWithStyleBuilderOptions{$name}{"Getter"};
  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $didCallSetValue = 0;
  if (exists $propertiesWithStyleBuilderOptions{$name}{"AutoFunctions"}) {
    $setterContent .= $indent . "    if (" . getAutoGetter($name, $parentStyle) . ") {\n";
    $setterContent .= $indent . "        " . getAutoSetter($name, $style) . ";\n";
    $setterContent .= $indent . "        return;\n";
    $setterContent .= $indent . "    }\n";
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"VisitedLinkColorSupport"}) {
    $setterContent .= $indent . "    Color color = " . $parentStyle . "->" . $getter . "();\n";
    if (!exists($propertiesWithStyleBuilderOptions{$name}{"NoDefaultColor"})) {
      $setterContent .= $indent . "    if (!color.isValid())\n";
      $setterContent .= $indent . "        color = " . $parentStyle . "->color();\n";
    }
    $setterContent .= generateColorValueSetter($name, "color", $indent . "    ");
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"AnimationProperty"}) {
    $setterContent .= generateAnimationPropertyInheritValueSetter($name, $indent . "    ");
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FontProperty"}) {
    $setterContent .= $indent . "    FontDescription fontDescription = styleResolver.fontDescription();\n";
    $setterContent .= $indent . "    fontDescription." . $setter . "(styleResolver.parentFontDescription()." . $getter . "());\n";
    $setterContent .= $indent . "    styleResolver.setFontDescription(fontDescription);\n";
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FillLayerProperty"}) {
    $setterContent .= generateFillLayerPropertyInheritValueSetter($name, $indent . "    ");
    $didCallSetValue = 1;
  }
  if (!$didCallSetValue) {
    my $inheritedValue = $parentStyle . "->" . ($isSVG ? "svgStyle()." : "") .  $getter . "()";
    $setterContent .= $indent . "    " . generateSetValueStatement($name, $inheritedValue) . ";\n";
  }
  $setterContent .= $indent . "}\n";

  return $setterContent;
}

sub generateValueSetter {
  my $name = shift;
  my $indent = shift;

  my $setterContent = "";
  $setterContent .= $indent . "static void applyValue" . $nameToId{$name} . "(StyleResolver& styleResolver, CSSValue& value)\n";
  $setterContent .= $indent . "{\n";
  my $convertedValue;
  if (exists($propertiesWithStyleBuilderOptions{$name}{"Converter"})) {
    $convertedValue = "StyleBuilderConverter::convert" . $propertiesWithStyleBuilderOptions{$name}{"Converter"} . "(styleResolver, value)";
  } elsif (exists($propertiesWithStyleBuilderOptions{$name}{"ConditionalConverter"})) {
    $setterContent .= $indent . "    auto convertedValue = StyleBuilderConverter::convert" . $propertiesWithStyleBuilderOptions{$name}{"ConditionalConverter"} . "(styleResolver, value);\n";
    $convertedValue = "convertedValue.value()";
  } else {
    $convertedValue = "downcast<CSSPrimitiveValue>(value)";
  }

  my $setter = $propertiesWithStyleBuilderOptions{$name}{"Setter"};
  my $style = "styleResolver.style()";
  my $didCallSetValue = 0;
  if (exists $propertiesWithStyleBuilderOptions{$name}{"AutoFunctions"}) {
    $setterContent .= $indent . "    if (downcast<CSSPrimitiveValue>(value).getValueID() == CSSValueAuto) {\n";
    $setterContent .= $indent . "        ". getAutoSetter($name, $style) . ";\n";
    $setterContent .= $indent . "        return;\n";
    $setterContent .= $indent . "    }\n";
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"VisitedLinkColorSupport"}) {
    $setterContent .= $indent . "    auto& primitiveValue = downcast<CSSPrimitiveValue>(value);\n";
    if ($name eq "color") {
      # The "color" property supports "currentColor" value. We should add a parameter.
      $setterContent .= handleCurrentColorValue($name, "primitiveValue", $indent . "    ");
    }
    $setterContent .= generateColorValueSetter($name, "primitiveValue", $indent . "    ", VALUE_IS_PRIMITIVE);
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"AnimationProperty"}) {
    $setterContent .= generateAnimationPropertyValueSetter($name, $indent . "    ");
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FontProperty"}) {
    $setterContent .= $indent . "    FontDescription fontDescription = styleResolver.fontDescription();\n";
    $setterContent .= $indent . "    fontDescription." . $setter . "(" . $convertedValue . ");\n";
    $setterContent .= $indent . "    styleResolver.setFontDescription(fontDescription);\n";
    $didCallSetValue = 1;
  } elsif (exists $propertiesWithStyleBuilderOptions{$name}{"FillLayerProperty"}) {
    $setterContent .= generateFillLayerPropertyValueSetter($name, $indent . "    ");
    $didCallSetValue = 1;
  }
  if (!$didCallSetValue) {
    if (exists($propertiesWithStyleBuilderOptions{$name}{"ConditionalConverter"})) {
      $setterContent .= $indent . "    if (convertedValue)\n";
      $setterContent .= "    ";
    }
    $setterContent .= $indent . "    " . generateSetValueStatement($name, $convertedValue) . ";\n";
  }
  $setterContent .= $indent . "}\n";

  return $setterContent;
}

open STYLEBUILDER, ">StyleBuilder.cpp" || die "Could not open StyleBuilder.cpp for writing";
print STYLEBUILDER << "EOF";
/* This file is automatically generated from CSSPropertyNames.in by makeprop, do not edit */

#include "config.h"
#include "StyleBuilder.h"

#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "RenderStyle.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderCustom.h"
#include "StylePropertyShorthand.h"
#include "StyleResolver.h"

namespace WebCore {

class StyleBuilderFunctions {
public:
EOF

foreach my $name (@names) {
  # Skip Shorthand properties and properties that do not use the StyleBuilder.
  next if (exists $propertiesWithStyleBuilderOptions{$name}{"Longhands"});
  next if (exists $propertiesWithStyleBuilderOptions{$name}{"SkipBuilder"});

  my $indent = "    ";
  if (!$propertiesWithStyleBuilderOptions{$name}{"Custom"}{"Initial"}) {
    print STYLEBUILDER generateInitialValueSetter($name, $indent);
  }
  if (!$propertiesWithStyleBuilderOptions{$name}{"Custom"}{"Inherit"}) {
    print STYLEBUILDER generateInheritValueSetter($name, $indent);
  }
  if (!$propertiesWithStyleBuilderOptions{$name}{"Custom"}{"Value"}) {
    print STYLEBUILDER generateValueSetter($name, $indent);
  }
}

print STYLEBUILDER << "EOF";
};

void StyleBuilder::applyProperty(CSSPropertyID property, StyleResolver& styleResolver, CSSValue& value, bool isInitial, bool isInherit)
{
    switch (property) {
    case CSSPropertyInvalid:
        break;
EOF

foreach my $name (@names) {
  print STYLEBUILDER "    case CSSProperty" . $nameToId{$name} . ":\n";
  if (exists $propertiesWithStyleBuilderOptions{$name}{"Longhands"}) {
    print STYLEBUILDER "        ASSERT(isExpandedShorthand(property));\n";
    print STYLEBUILDER "        ASSERT_NOT_REACHED();\n";
  } elsif (!exists $propertiesWithStyleBuilderOptions{$name}{"SkipBuilder"}) {
    print STYLEBUILDER "        if (isInitial)\n";
    print STYLEBUILDER "            " . getScopeForFunction($name, "Initial") . "::applyInitial" . $nameToId{$name} . "(styleResolver);\n";
    print STYLEBUILDER "        else if (isInherit)\n";
    print STYLEBUILDER "            " . getScopeForFunction($name, "Inherit") . "::applyInherit" . $nameToId{$name} . "(styleResolver);\n";
    print STYLEBUILDER "        else\n";
    print STYLEBUILDER "            " . getScopeForFunction($name, "Value") . "::applyValue" . $nameToId{$name} . "(styleResolver, value);\n";
  }
  print STYLEBUILDER "        break;\n";
}

print STYLEBUILDER << "EOF";
    };
}

} // namespace WebCore
EOF

close STYLEBUILDER;

# Generate StylePropertyShorthandsFunctions.
open SHORTHANDS_H, ">StylePropertyShorthandFunctions.h" || die "Could not open StylePropertyShorthandFunctions.h for writing";
print SHORTHANDS_H << "EOF";
/* This file is automatically generated from CSSPropertyNames.in by makeprop, do not edit */

#ifndef StylePropertyShorthandFunctions_h
#define StylePropertyShorthandFunctions_h

namespace WebCore {

class StylePropertyShorthand;

EOF

foreach my $name (@names) {
  # Skip non-Shorthand properties.
  next if (!exists $propertiesWithStyleBuilderOptions{$name}{"Longhands"});

  print SHORTHANDS_H "StylePropertyShorthand " . lcfirst($nameToId{$name}) . "Shorthand();\n";
}

print SHORTHANDS_H << "EOF";

} // namespace WebCore

#endif // StylePropertyShorthandFunctions_h
EOF

close SHORTHANDS_H;

open SHORTHANDS_CPP, ">StylePropertyShorthandFunctions.cpp" || die "Could not open StylePropertyShorthandFunctions.cpp for writing";
print SHORTHANDS_CPP << "EOF";
/* This file is automatically generated from CSSPropertyNames.in by makeprop, do not edit */

#include "config.h"
#include "StylePropertyShorthandFunctions.h"

#include "StylePropertyShorthand.h"

namespace WebCore {

EOF

foreach my $name (@names) {
  # Skip non-Shorthand properties.
  next if (!exists $propertiesWithStyleBuilderOptions{$name}{"Longhands"});

  my $lowercaseId = lcfirst($nameToId{$name});
  my @longhands = split(/\|/, $propertiesWithStyleBuilderOptions{$name}{"Longhands"});

  print SHORTHANDS_CPP "StylePropertyShorthand " . $lowercaseId . "Shorthand()\n";
  print SHORTHANDS_CPP "{\n";
  print SHORTHANDS_CPP "    static const CSSPropertyID " . $lowercaseId . "Properties[] = {\n";
  foreach (@longhands) {
    die "Unknown CSS property used in Longhands: " . $nameToId{$_} if !exists($nameToId{$_});
    print SHORTHANDS_CPP "        CSSProperty" . $nameToId{$_} . ",\n";
  }
  print SHORTHANDS_CPP "    };\n";
  print SHORTHANDS_CPP "    return StylePropertyShorthand(CSSProperty" . $nameToId{$name} . ", " . $lowercaseId . "Properties);\n";
  print SHORTHANDS_CPP "}\n\n";
}

print SHORTHANDS_CPP << "EOF";
} // namespace WebCore
EOF

close SHORTHANDS_CPP;

my $gperf = $ENV{GPERF} ? $ENV{GPERF} : "gperf";
system("\"$gperf\" --key-positions=\"*\" -D -n -s 2 CSSPropertyNames.gperf --output-file=CSSPropertyNames.cpp") == 0 || die "calling gperf failed: $?";
