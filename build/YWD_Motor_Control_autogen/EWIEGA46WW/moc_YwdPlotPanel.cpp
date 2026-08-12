/****************************************************************************
** Meta object code from reading C++ file 'YwdPlotPanel.hpp'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../YwdPlotPanel.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'YwdPlotPanel.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_YwdPlotPanel_t {
    QByteArrayData data[12];
    char stringdata0[134];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_YwdPlotPanel_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_YwdPlotPanel_t qt_meta_stringdata_YwdPlotPanel = {
    {
QT_MOC_LITERAL(0, 0, 12), // "YwdPlotPanel"
QT_MOC_LITERAL(1, 13, 12), // "requestClose"
QT_MOC_LITERAL(2, 26, 0), // ""
QT_MOC_LITERAL(3, 27, 13), // "YwdPlotPanel*"
QT_MOC_LITERAL(4, 41, 4), // "self"
QT_MOC_LITERAL(5, 46, 11), // "onTimerTick"
QT_MOC_LITERAL(6, 58, 12), // "onAddDiagram"
QT_MOC_LITERAL(7, 71, 13), // "onLiveToggled"
QT_MOC_LITERAL(8, 85, 2), // "on"
QT_MOC_LITERAL(9, 88, 21), // "onDiagramRequestClose"
QT_MOC_LITERAL(10, 110, 17), // "YwdDiagramWidget*"
QT_MOC_LITERAL(11, 128, 5) // "which"

    },
    "YwdPlotPanel\0requestClose\0\0YwdPlotPanel*\0"
    "self\0onTimerTick\0onAddDiagram\0"
    "onLiveToggled\0on\0onDiagramRequestClose\0"
    "YwdDiagramWidget*\0which"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_YwdPlotPanel[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    0,   42,    2, 0x0a /* Public */,
       6,    0,   43,    2, 0x08 /* Private */,
       7,    1,   44,    2, 0x08 /* Private */,
       9,    1,   47,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,    8,
    QMetaType::Void, 0x80000000 | 10,   11,

       0        // eod
};

void YwdPlotPanel::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<YwdPlotPanel *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->requestClose((*reinterpret_cast< YwdPlotPanel*(*)>(_a[1]))); break;
        case 1: _t->onTimerTick(); break;
        case 2: _t->onAddDiagram(); break;
        case 3: _t->onLiveToggled((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 4: _t->onDiagramRequestClose((*reinterpret_cast< YwdDiagramWidget*(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< YwdPlotPanel* >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< YwdDiagramWidget* >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (YwdPlotPanel::*)(YwdPlotPanel * );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&YwdPlotPanel::requestClose)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject YwdPlotPanel::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_YwdPlotPanel.data,
    qt_meta_data_YwdPlotPanel,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *YwdPlotPanel::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *YwdPlotPanel::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_YwdPlotPanel.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int YwdPlotPanel::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void YwdPlotPanel::requestClose(YwdPlotPanel * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
