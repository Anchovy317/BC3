#include "GastoReal.h"

#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

enum Columna { ColNombre = 0, ColGasto, ColAcumulado, ColPorcentaje, ColNota, NumColumnas };
enum Rol { RolMes = Qt::UserRole, RolPartida = Qt::UserRole + 1 };

QDate primerDia(const QDate& d) { return QDate(d.year(), d.month(), 1); }

QString nombreMes(const QDate& mes) {
    QString s = QLocale(QLocale::Spanish, QLocale::Spain).toString(mes, QStringLiteral("MMMM yyyy"));
    if (!s.isEmpty())
        s[0] = s[0].toUpper();
    return s;
}

QString normalizar(QString s) {
    s = s.toLower().normalized(QString::NormalizationForm_D);
    s.remove(QRegularExpression(QStringLiteral("\\p{M}")));
    return s;
}

// Acepta "1234.56", "1.234,56", "1234,56 €". Un punto solo se toma como
// separador de miles si va seguido de grupos de tres cifras ("1.234").
double leerImporte(QString s, bool* ok) {
    s.remove(QChar(0x20AC)); // €
    s.remove(QChar(0x00A0)); // espacio duro
    s.remove(QLatin1Char(' '));
    if (s.isEmpty()) {
        *ok = true;
        return 0;
    }
    if (s.contains(QLatin1Char(','))) {
        s.remove(QLatin1Char('.'));
        s.replace(QLatin1Char(','), QLatin1Char('.'));
    } else if (QRegularExpression(QStringLiteral("^-?\\d{1,3}(\\.\\d{3})+$")).match(s).hasMatch()) {
        s.remove(QLatin1Char('.'));
    }
    return s.toDouble(ok);
}

} // namespace

GastoRealWidget::GastoRealWidget(QWidget* parent) : QWidget(parent) {
    auto* raiz = new QVBoxLayout(this);
    raiz->setContentsMargins(16, 12, 16, 8);

    aviso_ = new QLabel(QStringLiteral("Abre un presupuesto (Archivo → Abrir JSON) para registrar el gasto real."));
    aviso_->setStyleSheet(QStringLiteral("color:#5d6b7a"));
    aviso_->setWordWrap(true);
    resumen_ = new QLabel;
    resumen_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont f = resumen_->font();
    f.setPointSize(f.pointSize() + 1);
    resumen_->setFont(f);

    auto* barra = new QHBoxLayout;
    bAnadir_ = new QPushButton(QStringLiteral("Añadir mes…"));
    bPartidas_ = new QPushButton(QStringLiteral("Desglosar por partidas…"));
    bQuitar_ = new QPushButton(QStringLiteral("Quitar"));
    bPartidas_->setToolTip(QStringLiteral("Añade partidas al mes seleccionado. Es opcional."));
    bQuitar_->setToolTip(QStringLiteral("Quita el mes o la partida seleccionados."));
    barra->addWidget(bAnadir_);
    barra->addWidget(bPartidas_);
    barra->addWidget(bQuitar_);
    barra->addStretch(1);

    arbol_ = new QTreeWidget;
    arbol_->setColumnCount(NumColumnas);
    arbol_->setHeaderLabels({QStringLiteral("Mes / partida"), QStringLiteral("Gasto"),
                             QStringLiteral("Acumulado"), QStringLiteral("% acumulado"),
                             QStringLiteral("Nota")});
    arbol_->setAlternatingRowColors(true);
    arbol_->setSelectionBehavior(QAbstractItemView::SelectRows);
    arbol_->setSelectionMode(QAbstractItemView::SingleSelection);
    arbol_->setEditTriggers(QAbstractItemView::NoEditTriggers); // se edita con doble clic, ver dobleClic()
    arbol_->setRootIsDecorated(true);
    arbol_->header()->setSectionResizeMode(ColNota, QHeaderView::Stretch);
    arbol_->setColumnWidth(ColNombre, 380);
    arbol_->setColumnWidth(ColGasto, 140);
    arbol_->setColumnWidth(ColAcumulado, 140);
    arbol_->setColumnWidth(ColPorcentaje, 110);

    raiz->addWidget(aviso_);
    raiz->addWidget(resumen_);
    raiz->addLayout(barra);
    raiz->addWidget(arbol_, 1);

    connect(bAnadir_, &QPushButton::clicked, this, &GastoRealWidget::anadirMes);
    connect(bPartidas_, &QPushButton::clicked, this, &GastoRealWidget::anadirPartidas);
    connect(bQuitar_, &QPushButton::clicked, this, &GastoRealWidget::quitar);
    connect(arbol_, &QTreeWidget::itemChanged, this, &GastoRealWidget::celdaEditada);
    connect(arbol_, &QTreeWidget::itemDoubleClicked, this, &GastoRealWidget::dobleClic);
    connect(arbol_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* it) {
        const int m = it->data(ColNombre, RolMes).toInt();
        if (!it->parent() && m >= 0 && m < meses_.size())
            expandidos_.insert(meses_.at(m).mes);
    });
    connect(arbol_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* it) {
        const int m = it->data(ColNombre, RolMes).toInt();
        if (!it->parent() && m >= 0 && m < meses_.size())
            expandidos_.remove(meses_.at(m).mes);
    });

    setEnabled(false);
    actualizarResumen();
}

void GastoRealWidget::setReferencia(const Presupuesto& p) {
    if (p.coste) {
        referencia_ = *p.coste;
        etiquetaReferencia_ = QStringLiteral("Presupuesto de coste");
    } else {
        referencia_ = p.total;
        etiquetaReferencia_ = QStringLiteral("Presupuesto");
    }

    // Partidas para el desglose. Si un código sale en varios capítulos, se suma lo presupuestado.
    partidas_.clear();
    indicePartida_.clear();
    for (const Fila& fila : p.filas) {
        if (fila.capitulo)
            continue;
        const double ref = p.coste ? fila.importeCoste.value_or(0) : fila.importe;
        const auto it = indicePartida_.constFind(fila.codigo);
        if (it != indicePartida_.constEnd()) {
            partidas_[it.value()].referencia += ref;
            continue;
        }
        PartidaRef r;
        r.codigo = fila.codigo;
        r.resumen = fila.resumen;
        r.busqueda = fila.busqueda;
        r.referencia = ref;
        indicePartida_.insert(r.codigo, partidas_.size());
        partidas_.push_back(r);
    }

    aviso_->setText(QStringLiteral("Doble clic en una celda para editarla. Si un mes tiene partidas, su gasto es "
                                   "la suma de ellas. Guarda con Archivo → Guardar proyecto (Ctrl+S)."));

    meses_.clear();
    expandidos_.clear();
    setEnabled(true);
    repintar(-1, -1);
}

void GastoRealWidget::desdeJson(const QJsonObject& raiz) {
    meses_.clear();
    expandidos_.clear();
    const QJsonArray arr = raiz.value(QStringLiteral("meses")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QDate d = QDate::fromString(o.value(QStringLiteral("mes")).toString(), QStringLiteral("yyyy-MM"));
        if (!d.isValid())
            continue;
        GastoMes g;
        g.mes = primerDia(d);
        g.importe = o.value(QStringLiteral("importe")).toDouble();
        g.nota = o.value(QStringLiteral("nota")).toString();
        const QJsonArray ps = o.value(QStringLiteral("partidas")).toArray();
        for (const QJsonValue& pv : ps) {
            const QJsonObject po = pv.toObject();
            GastoPartida gp;
            gp.codigo = po.value(QStringLiteral("codigo")).toString();
            if (gp.codigo.isEmpty())
                continue;
            gp.importe = po.value(QStringLiteral("importe")).toDouble();
            gp.nota = po.value(QStringLiteral("nota")).toString();
            g.partidas.push_back(gp);
        }
        meses_.push_back(g);
    }
    std::sort(meses_.begin(), meses_.end(), [](const GastoMes& a, const GastoMes& b) { return a.mes < b.mes; });
    for (const GastoMes& g : meses_) {
        if (!g.partidas.isEmpty())
            expandidos_.insert(g.mes);
    }
    repintar(-1, -1);
}

QJsonObject GastoRealWidget::aJson() const {
    QJsonArray arr;
    for (const GastoMes& g : meses_) {
        QJsonObject o;
        o.insert(QStringLiteral("mes"), g.mes.toString(QStringLiteral("yyyy-MM")));
        o.insert(QStringLiteral("importe"), g.importe);
        if (!g.nota.isEmpty())
            o.insert(QStringLiteral("nota"), g.nota);
        if (!g.partidas.isEmpty()) {
            QJsonArray ps;
            for (const GastoPartida& gp : g.partidas) {
                QJsonObject po;
                po.insert(QStringLiteral("codigo"), gp.codigo);
                po.insert(QStringLiteral("importe"), gp.importe);
                if (!gp.nota.isEmpty())
                    po.insert(QStringLiteral("nota"), gp.nota);
                ps.append(po);
            }
            o.insert(QStringLiteral("partidas"), ps);
        }
        arr.append(o);
    }
    QJsonObject raiz;
    raiz.insert(QStringLiteral("version"), 2);
    raiz.insert(QStringLiteral("meses"), arr);
    return raiz;
}

QString GastoRealWidget::nombrePartida(const QString& codigo) const {
    const auto it = indicePartida_.constFind(codigo);
    if (it == indicePartida_.constEnd())
        return codigo + QStringLiteral(" (no está en el presupuesto)");
    return codigo + QStringLiteral(" — ") + partidas_.at(it.value()).resumen;
}

bool GastoRealWidget::editable(const QTreeWidgetItem* item, int columna) const {
    if (columna == ColNota)
        return true;
    if (columna != ColGasto)
        return false;
    if (item->parent())
        return true; // partida
    const int m = item->data(ColNombre, RolMes).toInt();
    return m >= 0 && m < meses_.size() && meses_.at(m).partidas.isEmpty(); // mes sin desglose
}

void GastoRealWidget::repintar(int selMes, int selPartida) {
    if (selMes == -2) {
        selMes = -1;
        selPartida = -1;
        if (const QTreeWidgetItem* it = arbol_->currentItem()) {
            selMes = it->data(ColNombre, RolMes).toInt();
            selPartida = it->data(ColNombre, RolPartida).toInt();
        }
    }

    const QSignalBlocker bloqueo(arbol_);
    arbol_->clear();

    auto derecha = [](QTreeWidgetItem* it, int col) { it->setTextAlignment(col, Qt::AlignRight | Qt::AlignVCenter); };

    double acumulado = 0;
    QHash<QString, double> acumPartida;
    QTreeWidgetItem* aSeleccionar = nullptr;

    for (int m = 0; m < meses_.size(); ++m) {
        const GastoMes& g = meses_.at(m);
        const double totalMes = g.total();
        acumulado += totalMes;

        auto* item = new QTreeWidgetItem(arbol_);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        item->setData(ColNombre, RolMes, m);
        item->setData(ColNombre, RolPartida, -1);
        QString nombre = nombreMes(g.mes);
        if (!g.partidas.isEmpty())
            nombre += QStringLiteral("  (%1 partidas)").arg(g.partidas.size());
        item->setText(ColNombre, nombre);
        item->setText(ColGasto, formatoEuro(totalMes));
        item->setText(ColAcumulado, formatoEuro(acumulado));
        if (referencia_ > 0)
            item->setText(ColPorcentaje, formatoNumero(acumulado / referencia_ * 100.0, 2) + QStringLiteral(" %"));
        item->setText(ColNota, g.nota);
        derecha(item, ColGasto);
        derecha(item, ColAcumulado);
        derecha(item, ColPorcentaje);
        QFont negrita = item->font(ColNombre);
        negrita.setBold(true);
        item->setFont(ColNombre, negrita);
        if (!g.partidas.isEmpty())
            item->setFont(ColGasto, negrita);
        if (selMes == m && selPartida < 0)
            aSeleccionar = item;

        for (int p = 0; p < g.partidas.size(); ++p) {
            const GastoPartida& gp = g.partidas.at(p);
            acumPartida[gp.codigo] += gp.importe;
            const double acum = acumPartida.value(gp.codigo);
            const auto idx = indicePartida_.constFind(gp.codigo);
            const double ref = idx == indicePartida_.constEnd() ? 0.0 : partidas_.at(idx.value()).referencia;

            auto* hijo = new QTreeWidgetItem(item);
            hijo->setFlags(hijo->flags() | Qt::ItemIsEditable);
            hijo->setData(ColNombre, RolMes, m);
            hijo->setData(ColNombre, RolPartida, p);
            hijo->setText(ColNombre, nombrePartida(gp.codigo));
            hijo->setToolTip(ColNombre, nombrePartida(gp.codigo));
            hijo->setText(ColGasto, formatoEuro(gp.importe));
            hijo->setText(ColAcumulado, formatoEuro(acum)); // acumulado de esta partida
            if (ref > 0) {
                hijo->setText(ColPorcentaje, formatoNumero(acum / ref * 100.0, 2) + QStringLiteral(" %"));
                hijo->setToolTip(ColPorcentaje, QStringLiteral("De %1 presupuestados en esta partida").arg(formatoEuro(ref)));
            }
            hijo->setText(ColNota, gp.nota);
            derecha(hijo, ColGasto);
            derecha(hijo, ColAcumulado);
            derecha(hijo, ColPorcentaje);
            if (selMes == m && selPartida == p)
                aSeleccionar = hijo;
        }
        item->setExpanded(expandidos_.contains(g.mes));
    }

    if (aSeleccionar)
        arbol_->setCurrentItem(aSeleccionar);
    actualizarResumen();
}

void GastoRealWidget::actualizarResumen() {
    double gastado = 0;
    for (const GastoMes& g : meses_)
        gastado += g.total();
    const double pendiente = referencia_ - gastado;

    QStringList partes;
    partes << QStringLiteral("%1  %2").arg(etiquetaReferencia_.isEmpty() ? QStringLiteral("Presupuesto") : etiquetaReferencia_,
                                          formatoEuro(referencia_));
    partes << QStringLiteral("Gasto real  %1").arg(formatoEuro(gastado));
    partes << QStringLiteral("Pendiente  %1").arg(formatoEuro(pendiente));
    if (referencia_ > 0)
        partes << QStringLiteral("Ejecutado  %1 %").arg(formatoNumero(gastado / referencia_ * 100.0, 2));
    resumen_->setText(partes.join(QStringLiteral("      ")));
    // Rojo si el gasto ya supera lo presupuestado.
    resumen_->setStyleSheet(pendiente < 0 && referencia_ > 0 ? QStringLiteral("color:#b00020") : QString());
}

void GastoRealWidget::anadirMes() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Añadir mes"));
    auto* form = new QFormLayout(&dlg);
    auto* fecha = new QDateEdit;
    fecha->setDisplayFormat(QStringLiteral("MM/yyyy"));
    fecha->setCalendarPopup(true);
    fecha->setDate(meses_.isEmpty() ? primerDia(QDate::currentDate()) : meses_.last().mes.addMonths(1));
    form->addRow(QStringLiteral("Mes"), fecha);
    auto* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(botones);
    connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QDate mes = primerDia(fecha->date());
    const auto repetido = std::any_of(meses_.cbegin(), meses_.cend(), [&](const GastoMes& g) { return g.mes == mes; });
    if (repetido) {
        QMessageBox::information(this, QStringLiteral("Gasto real"),
                                 QStringLiteral("%1 ya está en la tabla.").arg(nombreMes(mes)));
        return;
    }
    GastoMes nuevo;
    nuevo.mes = mes;
    const auto pos = std::lower_bound(meses_.begin(), meses_.end(), nuevo,
                                      [](const GastoMes& a, const GastoMes& b) { return a.mes < b.mes; });
    const int fila = static_cast<int>(pos - meses_.begin());
    meses_.insert(pos, nuevo);
    emit modificado();
    repintar(fila, -1);
    if (QTreeWidgetItem* it = arbol_->topLevelItem(fila))
        arbol_->editItem(it, ColGasto);
}

void GastoRealWidget::anadirPartidas() {
    const QTreeWidgetItem* actual = arbol_->currentItem();
    if (!actual) {
        QMessageBox::information(this, QStringLiteral("Gasto real"),
                                 QStringLiteral("Selecciona primero el mes que quieres desglosar."));
        return;
    }
    if (partidas_.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Gasto real"),
                                 QStringLiteral("El presupuesto abierto no tiene partidas."));
        return;
    }
    const int m = actual->data(ColNombre, RolMes).toInt();
    if (m < 0 || m >= meses_.size())
        return;
    const GastoMes& g = meses_.at(m);

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Partidas de %1").arg(nombreMes(g.mes)));
    dlg.resize(680, 500);
    auto* lay = new QVBoxLayout(&dlg);
    auto* filtro = new QLineEdit;
    filtro->setPlaceholderText(QStringLiteral("Buscar por código, descripción, texto o capítulo"));
    filtro->setClearButtonEnabled(true);
    auto* lista = new QListWidget;
    lista->setSelectionMode(QAbstractItemView::ExtendedSelection);
    QSet<QString> yaEstan;
    for (const GastoPartida& gp : g.partidas)
        yaEstan.insert(gp.codigo);
    for (int i = 0; i < partidas_.size(); ++i) {
        const PartidaRef& r = partidas_.at(i);
        if (yaEstan.contains(r.codigo))
            continue;
        auto* it = new QListWidgetItem(r.codigo + QStringLiteral(" — ") + r.resumen, lista);
        it->setData(Qt::UserRole, r.codigo);
        it->setData(Qt::UserRole + 1, i);
    }
    auto* botones = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(new QLabel(QStringLiteral("Elige una o varias partidas (Ctrl o Mayús para varias).")));
    lay->addWidget(filtro);
    lay->addWidget(lista, 1);
    lay->addWidget(botones);
    connect(botones, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(botones, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(filtro, &QLineEdit::textChanged, &dlg, [this, lista](const QString& texto) {
        const QStringList palabras = normalizar(texto.trimmed()).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (int i = 0; i < lista->count(); ++i) {
            QListWidgetItem* it = lista->item(i);
            const QString& busq = partidas_.at(it->data(Qt::UserRole + 1).toInt()).busqueda;
            const bool ok = std::all_of(palabras.cbegin(), palabras.cend(),
                                        [&](const QString& w) { return busq.contains(w); });
            it->setHidden(!ok);
        }
    });
    if (dlg.exec() != QDialog::Accepted)
        return;

    QStringList nuevos;
    for (int i = 0; i < lista->count(); ++i) {
        const QListWidgetItem* it = lista->item(i);
        if (it->isSelected() && !it->isHidden())
            nuevos << it->data(Qt::UserRole).toString();
    }
    if (nuevos.isEmpty())
        return;

    GastoMes& mes = meses_[m];
    if (mes.partidas.isEmpty() && mes.importe != 0) {
        const auto resp = QMessageBox::question(
            this, QStringLiteral("Desglosar por partidas"),
            QStringLiteral("%1 tiene un gasto de %2 escrito a mano.\n"
                           "Al desglosar, el gasto del mes pasa a ser la suma de sus partidas y ese importe se descarta. ¿Continuar?")
                .arg(nombreMes(mes.mes), formatoEuro(mes.importe)));
        if (resp != QMessageBox::Yes)
            return;
        mes.importe = 0;
    }
    const int primera = mes.partidas.size();
    for (const QString& codigo : nuevos) {
        GastoPartida gp;
        gp.codigo = codigo;
        mes.partidas.push_back(gp);
    }
    expandidos_.insert(mes.mes);
    emit modificado();
    repintar(m, primera);
    if (QTreeWidgetItem* padre = arbol_->topLevelItem(m)) {
        if (QTreeWidgetItem* hijo = padre->child(primera))
            arbol_->editItem(hijo, ColGasto);
    }
}

void GastoRealWidget::quitar() {
    const QTreeWidgetItem* it = arbol_->currentItem();
    if (!it)
        return;
    const int m = it->data(ColNombre, RolMes).toInt();
    const int p = it->data(ColNombre, RolPartida).toInt();
    if (m < 0 || m >= meses_.size())
        return;
    GastoMes& g = meses_[m];

    if (p >= 0 && p < g.partidas.size()) {
        const GastoPartida& gp = g.partidas.at(p);
        if (gp.importe != 0 || !gp.nota.isEmpty()) {
            const auto resp = QMessageBox::question(
                this, QStringLiteral("Quitar partida"),
                QStringLiteral("¿Quitar %1 (%2) de %3?").arg(gp.codigo, formatoEuro(gp.importe), nombreMes(g.mes)));
            if (resp != QMessageBox::Yes)
                return;
        }
        g.partidas.removeAt(p);
        emit modificado();
        repintar(m, -1);
        return;
    }

    if (g.total() != 0 || !g.nota.isEmpty() || !g.partidas.isEmpty()) {
        const auto resp = QMessageBox::question(
            this, QStringLiteral("Quitar mes"),
            QStringLiteral("¿Quitar %1 (%2)?").arg(nombreMes(g.mes), formatoEuro(g.total())));
        if (resp != QMessageBox::Yes)
            return;
    }
    expandidos_.remove(g.mes);
    meses_.removeAt(m);
    emit modificado();
    repintar(-1, -1);
}

void GastoRealWidget::dobleClic(QTreeWidgetItem* item, int columna) {
    if (editable(item, columna))
        arbol_->editItem(item, columna);
}

void GastoRealWidget::celdaEditada(QTreeWidgetItem* item, int columna) {
    if (columna != ColGasto && columna != ColNota)
        return;
    const int m = item->data(ColNombre, RolMes).toInt();
    const int p = item->data(ColNombre, RolPartida).toInt();
    if (m < 0 || m >= meses_.size())
        return;
    GastoMes& g = meses_[m];
    const bool esPartida = p >= 0 && p < g.partidas.size();

    if (columna == ColGasto) {
        if (!editable(item, columna)) { // mes con partidas: su gasto sale de la suma
            QTimer::singleShot(0, this, [this] { repintar(); });
            return;
        }
        bool ok = false;
        const double v = leerImporte(item->text(ColGasto), &ok);
        if (!ok) {
            QMessageBox::warning(this, QStringLiteral("Gasto real"),
                                 QStringLiteral("«%1» no es un importe válido. Ejemplo: 12.345,67").arg(item->text(ColGasto)));
            QTimer::singleShot(0, this, [this] { repintar(); }); // devuelve el valor anterior
            return;
        }
        if (esPartida)
            g.partidas[p].importe = v;
        else
            g.importe = v;
        emit modificado();
        QTimer::singleShot(0, this, [this] { repintar(); }); // reformatea y recalcula acumulados
    } else {
        if (esPartida)
            g.partidas[p].nota = item->text(ColNota);
        else
            g.nota = item->text(ColNota);
        emit modificado();
    }
}
