#pragma once

#include "RundownTreeBaseWidget.h"

#include <QtGui/QUndoCommand>
#include <QtCore/QString>

// RAII helper: captures tree state before/after a destructive operation.
// Automatically pushes an undo command on destruction if the tree changed.
class UndoScope
{
    public:
        UndoScope(RundownTreeBaseWidget* tree, const QString& description)
            : m_tree(tree)
        {
            tree->beginUndoSnapshot(description);
        }

        ~UndoScope()
        {
            m_tree->endUndoSnapshot();
        }

    private:
        RundownTreeBaseWidget* m_tree;
        Q_DISABLE_COPY(UndoScope)
};

class TreeSnapshotCommand : public QUndoCommand
{
    public:
        TreeSnapshotCommand(RundownTreeBaseWidget* tree,
                            const QString& description,
                            const QString& beforeXml,
                            const QString& afterXml)
            : QUndoCommand(description)
            , m_tree(tree)
            , m_beforeXml(beforeXml)
            , m_afterXml(afterXml)
        {
        }

        void undo() override;
        void redo() override;

        int id() const override { return 1; }

    private:
        RundownTreeBaseWidget* m_tree;
        QString m_beforeXml;
        QString m_afterXml;
        bool m_firstRedo = true;
};
