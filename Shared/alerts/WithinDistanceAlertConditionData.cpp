// Copyright 2017 ESRI
//
// All rights reserved under the copyright laws of the United States
// and applicable international laws, treaties, and conventions.
//
// You may freely redistribute and use this sample code, with or
// without modification, provided you include the original copyright
// notice and use restrictions.
//
// See the Sample code usage restrictions document for further information.
//

#include "WithinDistanceAlertConditionData.h"

#include "GeoElement.h"
#include "Graphic.h"
#include "Point.h"

using namespace Esri::ArcGISRuntime;

WithinDistanceAlertConditionData::WithinDistanceAlertConditionData(AlertCondition* condition,
                                                                   AlertSource* source,
                                                                   AlertTarget* target,
                                                                   double distance):
  AlertConditionData(condition, source, target),
  m_distance(distance)
{

}

WithinDistanceAlertConditionData::~WithinDistanceAlertConditionData()
{

}

double WithinDistanceAlertConditionData::distance() const
{
  return m_distance;
}
