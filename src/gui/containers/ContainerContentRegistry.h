#pragma once
// no-port-check: NereusSDR-original content catalog and lossless meter adapter.
// Modification history (NereusSDR):
//   2026-10-08 - Workspace preview only: widget factories take a render
//                 mode, and createPreview passes the effective context and
//                 ContentRenderMode::Preview. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-02 — Mixed container ownership, persistence and source routing by
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "ContainerDocument.h"
#include <QObject>
#include <QMetaType>
#include <QPointer>
#include <QHash>
#include <QWidget>
#include <functional>
namespace NereusSDR {
class MeterItem;
enum class ContentRenderMode { Live, Preview, Validation };
struct ContentDescriptor {
    QString typeId, title;
    bool singleton = false, available = false;
    QString unavailableReason;
};
class ContainerContentRegistry : public QObject {
    Q_OBJECT
public:
    explicit ContainerContentRegistry(QObject* parent = nullptr);
    ~ContainerContentRegistry() override;
    void attachSingleton(const QString& typeId, QWidget* liveWidget);
    QWidget* singletonView(const QString& typeId) const;
    QSize singletonCanvasMinimum(const QString& typeId) const;
    QSize singletonCanvasSizeHint(const QString& typeId) const;
    void setAvailable(const QString& typeId, bool available, const QString& reason = {});
    bool isAvailable(const QString& typeId) const;
    QString unavailableReason(const QString& typeId) const;
    void parkSingleton(QWidget* widget);
    void returnBorrowedView(QWidget* widget);
    void setSingletonPlacements(const QHash<QString, QString>& placements);
    bool claimSingleton(const QString& typeId, const QString& entryId);
    quint64 generation() const { return m_generation; }
    static QString appletTypeForVisibilityId(const QString& id);
    QVector<ContentDescriptor> descriptors() const;
    ContentEntry makeEntry(const QString& typeId) const;
    QString validateEntry(const ContentEntry& entry) const;
    MeterItem* createMeterItem(const ContentEntry& entry, QObject* parent,
                              ContentRenderMode mode = ContentRenderMode::Live) const;
    QWidget* createPreview(const ContentEntry& entry, QWidget* parent) const;
    // As above, for an entry shown under `context`, the effective source
    // context (ContainerContentHost::effectiveContext) the live host uses.
    QWidget* createPreview(const ContentEntry& entry, QWidget* parent, const QJsonObject& context) const;
    ContentEntry captureMeterItem(const MeterItem& item, const ContentEntry& prior = {}) const;

    // Per-instance QWidget content (workspace preview: the Longpath
    // instruments). Unlike attachSingleton, the host creates one widget per
    // entry through the factory. The factory receives the entry with its
    // context replaced by the effective source context (container default
    // merged with the entry, see ContainerContentHost::effectiveContext),
    // and the parent the widget goes into. A factory returns nullptr when
    // it cannot build the entry; the host then shows its explanation row.
    // In ContentRenderMode::Preview the widget is inert: it registers no
    // reading consumer, follows no slice and runs no timer, and shows the
    // cached readings for its context.
    using WidgetFactory = std::function<QWidget*(const ContentEntry& entry, QWidget* parent,
                                                 ContentRenderMode mode)>;
    void registerWidgetFactory(const QString& typeId, const ContentDescriptor& descriptor, WidgetFactory factory);
    void unregisterWidgetFactory(const QString& typeId);
    bool hasWidgetFactory(const QString& typeId) const;
    QWidget* createWidget(const ContentEntry& entry, QWidget* parent,
                          ContentRenderMode mode = ContentRenderMode::Live) const;
signals:
    void runtimeChanged();
private:
    struct NativeSizes { QSize minimum, maximum, hint, editMinimum; };
    QHash<QWidget*,NativeSizes> m_nativeSizes;
    void restoreNativeConstraints(QWidget*);
    QHash<QString, QPointer<QWidget>> m_singletons;
    QHash<QWidget*, QPointer<QWidget>> m_originalParents;
    QHash<QString, QWidget*> m_attachedIdentities;
    QHash<QString, QMetaObject::Connection> m_destroyConnections;
    QHash<QString, QString> m_placements;
    QHash<QString, QString> m_unavailable;
    struct WidgetFactoryRecord { ContentDescriptor descriptor; WidgetFactory factory; };
    QHash<QString, WidgetFactoryRecord> m_widgetFactories;
    QStringList m_widgetFactoryOrder;
    QWidget* m_parking = nullptr;
    quint64 m_generation = 0;
};
} // namespace NereusSDR
Q_DECLARE_METATYPE(NereusSDR::ContentEntry)
