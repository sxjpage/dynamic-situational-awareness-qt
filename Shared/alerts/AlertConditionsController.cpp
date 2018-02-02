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

#include "AlertListModel.h"
#include "AttributeEqualsAlertCondition.h"
#include "FixedValueAlertTarget.h"
#include "AlertConditionData.h"
#include "AlertConditionListModel.h"
#include "AlertConditionsController.h"
#include "FeatureLayerAlertTarget.h"
#include "GeoElementAlertTarget.h"
#include "GraphicsOverlayAlertTarget.h"
#include "LocationAlertSource.h"
#include "WithinAreaAlertCondition.h"
#include "WithinDistanceAlertCondition.h"

#include "ToolManager.h"
#include "ToolResourceProvider.h"

#include "ArcGISFeatureTable.h"
#include "GeoView.h"
#include "GraphicsOverlay.h"
#include "GraphicsOverlayListModel.h"
#include "FeatureLayer.h"
#include "Layer.h"
#include "LayerListModel.h"

#include <QEventLoop>

using namespace Esri::ArcGISRuntime;

struct ResultsManager {

  QList<IdentifyLayerResult*>& m_results;

  ResultsManager(QList<IdentifyLayerResult*>& results):
    m_results(results)
  {
  }

  ~ResultsManager()
  {
    qDeleteAll(m_results);
  }
};

struct GraphicsResultsManager {

  QList<IdentifyGraphicsOverlayResult*>& m_results;

  GraphicsResultsManager(QList<IdentifyGraphicsOverlayResult*>& results):
    m_results(results)
  {
  }

  ~GraphicsResultsManager()
  {
    qDeleteAll(m_results);
  }
};

/*!
  \class AlertConditionsController
  \inherits Toolkit::AbstractTool
  \brief Tool controller for working with the conditions which can trigger alerts.

  Alerts are created when a given \l AlertCondition is met.

  This tool presents the list of currently active conditions and allows these to
  be deleted etc.

  This tool allows new \l AlertConditions of various types to be created.

  \sa AlertConditionListModel
  \sa WithinAreaAlertCondition
  \sa WithinDistanceAlertCondition
 */

/*!
  \brief Constructor taking an optional \a parent.
 */
AlertConditionsController::AlertConditionsController(QObject* parent /* = nullptr */):
  Toolkit::AbstractTool(parent),
  m_conditions(new AlertConditionListModel(this)),
  m_sourceNames(new QStringListModel(QStringList{"My Location"}, this)),
  m_targetNames(new QStringListModel(this)),
  m_levelNames(new QStringListModel(QStringList{"Low priority", "Moderate priority", "High priority", "Critical priority"},this)),
  m_locationSource(new LocationAlertSource(this))
{
  Toolkit::ToolManager::instance().addTool(this);

  connect(Toolkit::ToolResourceProvider::instance(), &Toolkit::ToolResourceProvider::geoViewChanged,
          this, &AlertConditionsController::onGeoviewChanged);

  connect(m_conditions, &AlertConditionListModel::dataChanged, this, &AlertConditionsController::conditionsListChanged);

  onGeoviewChanged();
}

/*!
  \brief Destructor.
 */
AlertConditionsController::~AlertConditionsController()
{
}

/*!
  \brief The name of this tool.
 */
QString AlertConditionsController::toolName() const
{
  return "Alert Conditions";
}

/*!
  \brief Sets the active state of this tool to \a active.

  \note When active this tool may handle clicks in the view for picking features or graphics.
 */
void AlertConditionsController::setActive(bool active)
{
  if (active == m_active)
    return;

  if (m_identifyLayersWatcher.isValid() && !m_identifyLayersWatcher.isDone() && !m_identifyLayersWatcher.isCanceled())
    m_identifyLayersWatcher.cancel();

  m_active = active;
  emit activeChanged();
}

/*!
  \brief Adds a \l WithinDistanceAlertCondition to the list of conditions.

  \list
    \li \a conditionName. The name for the condition.
    \li \a levelIndex. The \l AlertLevel for the condition.
    \li \a sourceFeedName. The name of the source feed (e.g. "My Location" or the
      name of a \l Esri::ArcGISRuntime::GraphicsOverlay) used to create an \l AlertSource.
    \li \a distance. The threshold distance for this cobdition in meters.
    \li \a itemId. The item id for the target. If \c -1, then all items in the target will be used.
      In the case where the target is a \l Esri::ArcGISRuntime::GraphicsOverlay, the  id should be the
      index of the graphic. In the case where the target is a \l Esri::ArcGISRuntime::FeatureLayer, the
      id should be the primary key value (e.g. the OID).
    \li \a targetOverlayIndex. The index of the target for the condition in the \l targetNames list. A target can be either
    a \l Esri::ArcGISRuntime::GraphicsOverlay or a \l Esri::ArcGISRuntime::FeatureLayer.
  \endlist
 */
void AlertConditionsController::addWithinDistanceAlert(const QString& conditionName,
                                                       int levelIndex,
                                                       const QString& sourceFeedName,
                                                       double distance,
                                                       int itemId,
                                                       int targetOverlayIndex)
{
  if (levelIndex < 0 ||
      sourceFeedName.isEmpty() ||
      distance < 0.0 ||
      targetOverlayIndex < 0)
    return;

  AlertLevel level = static_cast<AlertLevel>(levelIndex + 1);
  if (level > AlertLevel::Critical)
    return;

  AlertTarget* target = targetFromItemIdAndIndex(itemId, targetOverlayIndex);
  if (!target)
    return;

  WithinDistanceAlertCondition* condition = new WithinDistanceAlertCondition(level, conditionName, distance, this);
  connect(condition, &WithinDistanceAlertCondition::newConditionData, this, &AlertConditionsController::handleNewAlertConditionData);

  if (sourceFeedName == "My Location")
  {
    condition->init(m_locationSource, target);
  }
  else
  {
    GraphicsOverlay* sourceOverlay = graphicsOverlayFromName(sourceFeedName);
    if (sourceOverlay)
      condition->init(sourceOverlay, target);
  }

  m_conditions->addAlertCondition(condition);
}

/*!
  \brief Adds a \l WithinAreaAlertCondition to the list of conditions.

  \list
    \li \a conditionName. The name for the condition.
    \li \a levelIndex. The \l AlertLevel for the condition.
    \li \a sourceFeedName. The name of the source feed (e.g. "My Location" or the
      name of a \l Esri::ArcGISRuntime::GraphicsOverlay) used to create an \l AlertSource.
    \li \a itemId. The item id for the target. If \c -1, then all items in the target will be used.
      In the case where the target is a \l Esri::ArcGISRuntime::GraphicsOverlay, the  id should be the
      index of the graphic. In the case where the target is a \l Esri::ArcGISRuntime::FeatureLayer, the
      id should be the primary key value (e.g. the OID).
    \li \a targetOverlayIndex. The index of the target for the condition in the \l targetNames list. A target can be either
    a \l Esri::ArcGISRuntime::GraphicsOverlay or a \l Esri::ArcGISRuntime::FeatureLayer.
  \endlist
 */
void AlertConditionsController::addWithinAreaAlert(const QString& conditionName,
                                                   int levelIndex,
                                                   const QString& sourceFeedName,
                                                   int itemId,
                                                   int targetOverlayIndex)
{
  if (levelIndex < 0 ||
      sourceFeedName.isEmpty() ||
      targetOverlayIndex < 0)
    return;

  AlertLevel level = static_cast<AlertLevel>(levelIndex + 1);
  if (level > AlertLevel::Critical)
    return;

  AlertTarget* target = targetFromItemIdAndIndex(itemId, targetOverlayIndex);
  if (!target)
    return;

  WithinAreaAlertCondition* condition = new WithinAreaAlertCondition(level, conditionName, this);
  connect(condition, &WithinAreaAlertCondition::newConditionData, this, &AlertConditionsController::handleNewAlertConditionData);

  if (sourceFeedName == "My Location")
  {
    condition->init(m_locationSource, target);
  }
  else
  {
    GraphicsOverlay* sourceOverlay = graphicsOverlayFromName(sourceFeedName);
    if (sourceOverlay)
      condition->init(sourceOverlay, target);
  }
  m_conditions->addAlertCondition(condition);
}

/*!
  \brief Adds a \l AttributeEqualsAlertCondition to the list of conditions.

  \list
    \li \a conditionName. The name for the condition.
    \li \a levelIndex. The \l AlertLevel for the condition.
    \li \a sourceFeedName. The name of the source feed
      (e.g. a \l Esri::ArcGISRuntime::GraphicsOverlay) used to create an \l AlertSource.
    \li attributeName. The name of the attribute to query.
    \li targetValue. The attribute value to check for.
  \endlist
 */
void AlertConditionsController::addAttributeEqualsAlert(const QString& conditionName,
                                                        int levelIndex,
                                                        const QString& sourceFeedName,
                                                        const QString& attributeName,
                                                        const QVariant& targetValue)
{
  if (levelIndex < 0 ||
      sourceFeedName.isEmpty() ||
      attributeName.isEmpty() ||
      targetValue.isNull())
    return;

  AlertLevel level = static_cast<AlertLevel>(levelIndex + 1);
  if (level > AlertLevel::Critical)
    return;

  AlertTarget* target = new FixedValueAlertTarget(targetValue, this);

  GraphicsOverlay* sourceOverlay = graphicsOverlayFromName(sourceFeedName);
  if (!sourceOverlay)
    return;

  AttributeEqualsAlertCondition* condition = new AttributeEqualsAlertCondition(level, conditionName, attributeName, this);
  connect(condition, &AttributeEqualsAlertCondition::newConditionData, this, &AlertConditionsController::handleNewAlertConditionData);
  condition->init(sourceOverlay, target);
  m_conditions->addAlertCondition(condition);
}

/*!
  \brief Removes the condition at \a rowIndex from the list.
 */
void AlertConditionsController::removeConditionAt(int rowIndex)
{
  AlertCondition* condition = m_conditions->conditionAt(rowIndex);
  m_conditions->removeAt(rowIndex);
  delete condition;
}

/*!
  \brief Toggle the pick mode of the tool.

  When \c true, the tool will attempt to pick a graphic or feature if the user clicks
  in the view.

  \sa pickMode
 */
void AlertConditionsController::togglePickMode()
{
  m_pickMode = !m_pickMode;

  if (m_pickMode)
  {
    m_mouseClickConnection = connect(Toolkit::ToolResourceProvider::instance(), &Toolkit::ToolResourceProvider::mouseClicked,
                                     this, &AlertConditionsController::onMouseClicked);

    m_identifyLayersConnection =  connect(Toolkit::ToolResourceProvider::instance(), &Toolkit::ToolResourceProvider::identifyLayersCompleted,
                                          this, &AlertConditionsController::onIdentifyLayersCompleted);

    m_identifyGraphicsConnection =  connect(Toolkit::ToolResourceProvider::instance(), &Toolkit::ToolResourceProvider::identifyGraphicsOverlaysCompleted,
                                            this, &AlertConditionsController::onIdentifyGraphicsOverlaysCompleted);
  }
  else
  {
    disconnect(m_mouseClickConnection);
    disconnect(m_identifyLayersConnection);
    disconnect(m_identifyGraphicsConnection);
  }

  emit pickModeChanged();
}

/*!
  \brief Returns a QAbstractItemModel containing the list of
  names for creating condition sources.

  \sa AlertSource
 */
QAbstractItemModel* AlertConditionsController::sourceNames() const
{
  return m_sourceNames;
}

/*!
  \brief Returns a QAbstractItemModel containing the list of
  names for creating condition targets.

  \sa AlertTarget
 */
QAbstractItemModel* AlertConditionsController::targetNames() const
{
  return m_targetNames;
}

/*!
  \brief Returns a QAbstractItemModel containing the list of
  level names for creating condition targets.

  \sa AlertLevel
 */
QAbstractItemModel* AlertConditionsController::levelNames() const
{
  return m_levelNames;
}

/*!
  \brief Returns a QAbstractItemModel containing the list of
  conditions.

  \sa AlertCondition
 */
QAbstractItemModel* AlertConditionsController::conditionsList() const
{
  return m_conditions;
}

/*!
  \brief Returns whether the tool is in pick mode or not.
 */
bool AlertConditionsController::pickMode() const
{
  return m_pickMode;
}

/*!
  \brief internal

  Handle changes to the geoView.
 */
void AlertConditionsController::onGeoviewChanged()
{
  setTargetNames(QStringList());
  setSourceNames(QStringList("My Location"));

  GeoView* geoView = Toolkit::ToolResourceProvider::instance()->geoView();
  if (!geoView)
    return;

  LayerListModel* operationalLayers = Toolkit::ToolResourceProvider::instance()->operationalLayers();
  if (operationalLayers)
  {
    connect(operationalLayers, &LayerListModel::layerAdded, this, &AlertConditionsController::onLayersChanged);
    connect(operationalLayers, &LayerListModel::layerRemoved, this, &AlertConditionsController::onLayersChanged);
  }

  GraphicsOverlayListModel* graphicsOverlays = geoView->graphicsOverlays();
  if (graphicsOverlays)
  {
    connect(graphicsOverlays, &GraphicsOverlayListModel::graphicsOverlayAdded, this, &AlertConditionsController::onLayersChanged);
    connect(graphicsOverlays, &GraphicsOverlayListModel::graphicsOverlayRemoved, this, &AlertConditionsController::onLayersChanged);
  }

  onLayersChanged();
}

/*!
  \brief internal

  Handle changes to the list of layers and graphics overlays in the geoView.

  These data types are the underlying data for \l AlertSource and \l AlertTarget.
 */
void AlertConditionsController::onLayersChanged()
{
  GeoView* geoView = Toolkit::ToolResourceProvider::instance()->geoView();
  if (!geoView)
  {
    setTargetNames(QStringList());
    setSourceNames(QStringList("My Location"));
    return;
  }

  QStringList newTargetList;
  QStringList existingLayerIds;
  LayerListModel* operationalLayers = Toolkit::ToolResourceProvider::instance()->operationalLayers();
  if (operationalLayers)
  {
    const int opLayersCount = operationalLayers->rowCount();
    for (int i = 0; i < opLayersCount; ++i)
    {
      Layer* lyr = operationalLayers->at(i);
      if (!lyr)
        continue;

      FeatureLayer* featLayer = qobject_cast<FeatureLayer*>(lyr);
      if (!featLayer)
        continue;

      if (featLayer->loadStatus() != LoadStatus::Loaded)
        connect(featLayer, &FeatureLayer::doneLoading, this, &AlertConditionsController::onLayersChanged);
      else
      {
        newTargetList.append(featLayer->name());
        existingLayerIds.append(featLayer->layerId());
      }
    }
  }

  const auto cachedLayerTargetKeys = m_layerTargets.keys();
  for (const QString& key : cachedLayerTargetKeys)
  {
    if (!existingLayerIds.contains(key))
      m_layerTargets.remove(key);
  }

  QStringList newSourceList{"My Location"};
  GraphicsOverlayListModel* graphicsOverlays = geoView->graphicsOverlays();
  if (graphicsOverlays)
  {
    const int overlaysCount = graphicsOverlays->rowCount();
    for (int i = 0; i < overlaysCount; ++i)
    {
      GraphicsOverlay* overlay = graphicsOverlays->at(i);
      if (!overlay)
        continue;

      if (overlay->overlayId().isEmpty())
        continue;

      if (realtimeFeedNames().contains(overlay->overlayId()))
        newSourceList.append(overlay->overlayId());

      newTargetList.append(overlay->overlayId());
    }
  }

  const auto cachedOverlayTargetKeys = m_overlayTargets.keys();
  for (const QString& key : cachedOverlayTargetKeys)
  {
    if (!newSourceList.contains(key))
      m_overlayTargets.remove(key);
  }

  setSourceNames(newSourceList);
  setTargetNames(newTargetList);
}

/*!
  \brief internal

  Handle mouse click events in the view. If active, this will attenpt to pick
  graphics or features.
 */
void AlertConditionsController::onMouseClicked(QMouseEvent &event)
{
  if (!isActive())
    return;

  if (event.button() != Qt::MouseButton::LeftButton)
    return;

  if (!m_pickMode)
    return;

  if (m_identifyLayersWatcher.isValid() && !m_identifyLayersWatcher.isDone())
    return;

  if (m_identifyGraphicsWatcher.isValid() && !m_identifyGraphicsWatcher.isDone())
    return;

  GeoView* geoView = Toolkit::ToolResourceProvider::instance()->geoView();
  if (!geoView)
    return;

  m_identifyLayersWatcher = geoView->identifyLayers(event.pos().x(), event.pos().y(), m_tolerance, false);
  m_identifyGraphicsWatcher = geoView->identifyGraphicsOverlays(event.pos().x(), event.pos().y(), m_tolerance, false);

  event.accept();
}

/*!
  \brief internal

  Handle the result of an identify layers task.
 */
void AlertConditionsController::onIdentifyLayersCompleted(const QUuid& taskId, QList<IdentifyLayerResult*> identifyResults)
{
  if (taskId != m_identifyLayersWatcher.taskId())
    return;

  ResultsManager resultsManager(identifyResults);

  if (!isActive())
    return;

  m_identifyLayersWatcher = TaskWatcher();

  auto it = resultsManager.m_results.begin();
  auto itEnd = resultsManager.m_results.end();
  for (; it != itEnd; ++it)
  {
    IdentifyLayerResult* res = *it;
    if (!res)
      continue;

    const QString layerName = res->layerContent()->name();

    const QList<GeoElement*> geoElements = res->geoElements();
    auto geoElemIt = geoElements.begin();
    auto geoElemEnd = geoElements.end();
    for (; geoElemIt != geoElemEnd; ++geoElemIt)
    {
      GeoElement* geoElement = *geoElemIt;
      if (!geoElement)
        continue;

      AttributeListModel* atts = geoElement->attributes();
      if (!atts)
        return;

      Feature* feature = qobject_cast<Feature*>(geoElement);
      if (!feature)
        continue;

      FeatureTable* table = feature->featureTable();
      if (!table)
        continue;

      QString primaryKeyField = primaryKeyFieldName(table);

      if (primaryKeyField.isEmpty())
        continue;

      if (!atts->containsAttribute(primaryKeyField))
        continue;

      m_identifyGraphicsWatcher.cancel();
      m_identifyGraphicsWatcher = TaskWatcher();
      emit pickedElement(layerName, atts->attributeValue(primaryKeyField).toInt());

      break;
    }
  }
}

/*!
  \brief internal

  Handle the result of an identify graphic overlays task.
 */
void AlertConditionsController::onIdentifyGraphicsOverlaysCompleted(const QUuid& taskId, QList<IdentifyGraphicsOverlayResult*> identifyResults)
{
  if (taskId != m_identifyGraphicsWatcher.taskId())
    return;

  GraphicsResultsManager resultsManager(identifyResults);

  if (!isActive())
    return;

  m_identifyGraphicsWatcher = TaskWatcher();

  auto it = resultsManager.m_results.begin();
  auto itEnd = resultsManager.m_results.end();
  for (; it != itEnd; ++it)
  {
    IdentifyGraphicsOverlayResult* res = *it;
    if (!res)
      continue;

    const QString overlayName = res->graphicsOverlay()->overlayId();

    const QList<Graphic*> graphics = res->graphics();
    auto graphicsIt = graphics.begin();
    auto graphicsEnd = graphics.end();
    for (; graphicsIt != graphicsEnd; ++graphicsIt)
    {
      Graphic* graphic = *graphicsIt;
      if (!graphic || !graphic->graphicsOverlay() || !graphic->graphicsOverlay()->graphics())
        continue;

      const int index = graphic->graphicsOverlay()->graphics()->indexOf(graphic);

      m_identifyLayersWatcher.cancel();
      m_identifyLayersWatcher = TaskWatcher();
      emit pickedElement(overlayName, index);

      break;
    }
  }
}

void AlertConditionsController::handleNewAlertConditionData(AlertConditionData* newConditionData)
{
  if (!newConditionData)
    return;

  AlertListModel::instance()->addAlertConditionData(newConditionData);
}

/*!
  \brief internal

  Set the list of names for targets to be used when creating conditions.

  \sa AlertCondition
  \sa AlertTarget
 */
void AlertConditionsController::setTargetNames(const QStringList& targetNames)
{
  const QStringList existingNames = m_targetNames->stringList();
  if (existingNames == targetNames)
    return;

  m_targetNames->setStringList(targetNames);
  emit targetNamesChanged();
}

/*!
  \brief internal

  Set the list of names for sources to be used when creating conditions.

  \sa AlertCondition
  \sa AlertSource
 */
void AlertConditionsController::setSourceNames(const QStringList& sourceNames)
{
  const QStringList existingNames = m_sourceNames->stringList();
  if (existingNames == sourceNames)
    return;

  m_sourceNames->setStringList(sourceNames);
  emit sourceNamesChanged();
}

/*!
  \brief internal
 */
AlertTarget* AlertConditionsController::targetFromItemIdAndIndex(int itemId, int targetOverlayIndex) const
{
  GeoView* geoView = Toolkit::ToolResourceProvider::instance()->geoView();
  if (!geoView)
    return nullptr;

  int currIndex = -1;
  LayerListModel* operationalLayers = Toolkit::ToolResourceProvider::instance()->operationalLayers();
  if (operationalLayers)
  {
    const int opLayersCount = operationalLayers->rowCount();
    for (int i = 0; i < opLayersCount; ++i)
    {
      currIndex++;
      Layer* layer = operationalLayers->at(i);
      if (!layer)
        continue;

      FeatureLayer* featLayer = qobject_cast<FeatureLayer*>(layer);
      if (!featLayer)
        continue;

      if (currIndex == targetOverlayIndex)
      {
        if (itemId == -1)
        {
          if (!m_layerTargets.contains(featLayer->layerId()))
            m_layerTargets.insert(featLayer->layerId(), new FeatureLayerAlertTarget(featLayer));

          return m_layerTargets.value(featLayer->layerId(), nullptr);
        }
        else
          return targetFromFeatureLayer(featLayer, itemId);
      }
    }
  }

  GraphicsOverlayListModel* graphicsOverlays = geoView->graphicsOverlays();
  if (graphicsOverlays)
  {
    const int overlaysCount = graphicsOverlays->rowCount();
    for (int i = 0; i < overlaysCount; ++i)
    {
      GraphicsOverlay* overlay = graphicsOverlays->at(i);
      if (!overlay)
        continue;

      if (overlay->overlayId().isEmpty())
        continue;

      ++currIndex;

      if (currIndex == targetOverlayIndex)
      {
        if (itemId == -1)
        {
          if (!m_overlayTargets.contains(overlay->overlayId()))
            m_overlayTargets.insert(overlay->overlayId(), new GraphicsOverlayAlertTarget(overlay));

          return m_overlayTargets.value(overlay->overlayId(), nullptr);
        }
        else
          return targetFromGraphicsOverlay(overlay, itemId);
      }
    }
  }

  return nullptr;
}

/*!
  \brief internal
 */
AlertTarget* AlertConditionsController::targetFromFeatureLayer(FeatureLayer* featureLayer, int itemId) const
{
  FeatureTable* tab = featureLayer->featureTable();
  if (!tab)
    return nullptr;

  const QString primaryKey = primaryKeyFieldName(tab);

  QueryParameters qp;
  qp.setWhereClause(QString("\"%1\" = %2").arg(primaryKey, QString::number(itemId)));

  QEventLoop loop;
  tab->queryFeatures(qp);

  connect(tab, &FeatureTable::errorOccurred, this, [this, &loop](Error)
  {
    loop.quit();
  });

  Feature* feature = nullptr;
  auto connection = loop.connect(tab, &FeatureTable::queryFeaturesCompleted, this, [this, &loop, &feature](QUuid, FeatureQueryResult* featureQueryResult)
  {
    loop.quit();

    if (featureQueryResult)
      feature = featureQueryResult->iterator().next();
  });

  loop.exec();

  disconnect(connection);

  if (!feature)
    return nullptr;

  return new GeoElementAlertTarget(feature);
}

/*!
  \brief internal
 */
AlertTarget* AlertConditionsController::targetFromGraphicsOverlay(GraphicsOverlay* graphicsOverlay, int itemId) const
{
  if (!graphicsOverlay)
    return nullptr;

  GraphicListModel* graphics = graphicsOverlay->graphics();
  if (!graphics)
    return nullptr;

  Graphic* g = graphics->at(itemId);
  if (!g)
    return nullptr;

  return new GeoElementAlertTarget(g);
}

/*!
  \brief internal
 */
GraphicsOverlay* AlertConditionsController::graphicsOverlayFromName(const QString& overlayName)
{
  GeoView* geoView = Toolkit::ToolResourceProvider::instance()->geoView();
  if (!geoView)
    return nullptr;

  GraphicsOverlayListModel* graphicsOverlays = geoView->graphicsOverlays();
  if (graphicsOverlays)
  {
    const int overlaysCount = graphicsOverlays->rowCount();
    for (int i = 0; i < overlaysCount; ++i)
    {
      GraphicsOverlay* overlay = graphicsOverlays->at(i);
      if (!overlay)
        continue;

      if (overlay->overlayId().isEmpty())
        continue;

      if (overlayName == overlay->overlayId())
        return overlay;
    }
  }

  return nullptr;
}

/*!
  \brief internal
 */
QString AlertConditionsController::primaryKeyFieldName(FeatureTable* featureTable) const
{
  if (!featureTable)
    return QString();

  ArcGISFeatureTable* agsFeatureTable = qobject_cast<ArcGISFeatureTable*>(featureTable);
  if (agsFeatureTable)
  {
    return agsFeatureTable->objectIdField();
  }
  else
  {
    const QList<Field> fields = featureTable->fields();
    for( const Field& field : fields)
    {
      if (field.fieldType() == FieldType::OID)
        return field.name();
    }
  }

  return QString();
}

/*!
  \brief internal
 */
QStringList AlertConditionsController::realtimeFeedNames()
{
  return QStringList{"cot", "position_report"};
}
