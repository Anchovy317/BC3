#include "ModeloPresupuesto.h"

#include <QFont>
#include <QRegularExpression>

#include <algorithm>

ModeloPresupuesto::ModeloPresupuesto(QObject* parent) : QAbstractItemModel(parent) {}

void ModeloPresupuesto::setPresupuesto(const Presupuesto* presupuesto) {
    beginResetModel();
    presupuesto_ = presupuesto;
    endResetModel();
}

int ModeloPresupuesto::filaId(const QModelIndex& index) const {
    return index.isValid() ? static_cast<int>(index.internalId()) : -1;
}

const Fila* ModeloPresupuesto::filaEn(const QModelIndex& index) const {
    const int id = filaId(index);
    if (!presupuesto_ || id < 0 || id >= presupuesto_->filas.size())
        return nullptr;
    return &presupuesto_->filas.at(id);
}

int ModeloPresupuesto::rowCount(const QModelIndex& parent) const {
    if (!presupuesto_ || parent.column() > 0)
        return 0;
    if (!parent.isValid())
        return presupuesto_->raices.size();
    const Fila* f = filaEn(parent);
    return f ? f->hijos.size() : 0;
}

int ModeloPresupuesto::columnCount(const QModelIndex&) const {
    return 7;
}

QModelIndex ModeloPresupuesto::index(int row, int column, const QModelIndex& parent) const {
    if (!presupuesto_ || column < 0 || column >= 7 || row < 0)
        return {};
    const QVector<int>* hijos = parent.isValid() ? &filaEn(parent)->hijos : &presupuesto_->raices;
    if (!hijos || row >= hijos->size())
        return {};
    return createIndex(row, column, static_cast<quintptr>(hijos->at(row)));
}

QModelIndex ModeloPresupuesto::parent(const QModelIndex& child) const {
    const Fila* f = filaEn(child);
    if (!f || f->padre < 0)
        return {};
    return indiceDe(f->padre);
}

QModelIndex ModeloPresupuesto::indiceDe(int filaId) const {
    if (!presupuesto_ || filaId < 0 || filaId >= presupuesto_->filas.size())
        return {};
    const Fila& f = presupuesto_->filas.at(filaId);
    const QVector<int>& hermanos = f.padre < 0 ? presupuesto_->raices : presupuesto_->filas.at(f.padre).hijos;
    const int row = hermanos.indexOf(filaId);
    if (row < 0)
        return {};
    return createIndex(row, 0, static_cast<quintptr>(filaId));
}

QVariant ModeloPresupuesto::data(const QModelIndex& index, int role) const {
    const Fila* f = filaEn(index);
    if (!f)
        return {};

    const double valor = f->capitulo ? f->total : f->importe;
    const double pct = presupuesto_->total > 0 ? valor / presupuesto_->total * 100.0 : 0;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return f->codigo;
        case 1:
            if (f->desajuste)
                return f->resumen + QStringLiteral("    ⚠ ~M ") + formatoNumero(f->medicionTotal.value_or(0));
            return f->resumen;
        case 2: return f->capitulo ? QVariant() : QVariant(f->unidad);
        case 3: return f->capitulo ? QVariant() : QVariant(formatoNumero(f->cantidad));
        case 4: return (!f->capitulo && f->precio) ? QVariant(formatoNumero(*f->precio, 2)) : QVariant();
        case 5: return formatoEuro(valor);
        case 6: return pct > 0 ? formatoNumero(pct, 2) + QStringLiteral(" %") : QVariant();
        }
    }
    if (role == Qt::TextAlignmentRole && index.column() >= 3)
        return int(Qt::AlignRight | Qt::AlignVCenter);
    if (role == Qt::FontRole && f->capitulo) {
        QFont fuente;
        fuente.setBold(true);
        return fuente;
    }
    if (role == Qt::ToolTipRole && f->desajuste) {
        return QStringLiteral("La medición ~M (%1) no coincide con la cantidad del ~D (%2). El importe usa la cantidad del ~D.")
            .arg(formatoNumero(f->medicionTotal.value_or(0)), formatoNumero(f->cantidad));
    }
    if (role == Qt::UserRole)
        return f->id;
    return {};
}

QVariant ModeloPresupuesto::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case 0: return QStringLiteral("Código");
    case 1: return QStringLiteral("Resumen");
    case 2: return QStringLiteral("Ud");
    case 3: return QStringLiteral("Cantidad");
    case 4: return QStringLiteral("Precio");
    case 5: return QStringLiteral("Importe");
    case 6: return QStringLiteral("% del total");
    }
    return {};
}

Qt::ItemFlags ModeloPresupuesto::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return {};
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

FiltroPresupuesto::FiltroPresupuesto(QObject* parent) : QSortFilterProxyModel(parent) {
    setRecursiveFilteringEnabled(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

bool FiltroPresupuesto::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    const QString texto = filterRegularExpression().pattern();
    if (texto.isEmpty())
        return true;
    const auto* modelo = qobject_cast<const ModeloPresupuesto*>(sourceModel());
    if (!modelo)
        return true;
    const Fila* f = modelo->filaEn(modelo->index(sourceRow, 0, sourceParent));
    if (!f || f->capitulo)
        return false; // el padre entra solo si un hijo casa, vía filtrado recursivo
    const QStringList palabras = texto.split(' ', Qt::SkipEmptyParts);
    return std::all_of(palabras.cbegin(), palabras.cend(), [&](const QString& w) {
        return f->busqueda.contains(w.toLower().normalized(QString::NormalizationForm_D).remove(QRegularExpression("\\p{M}")));
    });
}
