#pragma once

#include "Presupuesto.h"

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

// Árbol de capítulos y partidas. La vista solo pide las filas visibles,
// así un presupuesto largo no se repinta entero como el HTML.
class ModeloPresupuesto : public QAbstractItemModel {
    Q_OBJECT
public:
    explicit ModeloPresupuesto(QObject* parent = nullptr);

    void setPresupuesto(const Presupuesto* presupuesto);

    int filaId(const QModelIndex& index) const;
    const Fila* filaEn(const QModelIndex& index) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    const Presupuesto* presupuesto_ = nullptr;
    QModelIndex indiceDe(int filaId) const;
};

// Acepta una partida si todas las palabras están en código, resumen, texto o ruta.
// Con filtrado recursivo, el capítulo padre se queda para no perder el sitio.
class FiltroPresupuesto : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FiltroPresupuesto(QObject* parent = nullptr);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};
