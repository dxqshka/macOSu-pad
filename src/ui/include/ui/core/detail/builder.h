#pragma once

#define UI_FLUENT_FIELD(_self, _type, _field, _setField, _onSet) \
    constexpr _type _field() const {                             \
        return _##_field;                                        \
    }                                                            \
    constexpr _self _field(_type value) const {                  \
        auto result = *this;                                     \
        result._setField(value);                                 \
        return result;                                           \
    }                                                            \
    constexpr void _setField(_type value) {                      \
        _##_field = value;                                       \
        _onSet;                                                  \
    }
