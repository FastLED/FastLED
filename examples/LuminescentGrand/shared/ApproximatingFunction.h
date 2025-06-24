// Copyleft (c) 2012, Zach Vorhies
// Public domain, no rights reserved.

#ifndef APPROXIMATING_FUNCTION_H_
#define APPROXIMATING_FUNCTION_H_

//#include <Arduino.h>

template<typename X, typename Y>
const Y MapT(const X& x,
             const X& x1, const X& x2,
             const Y& y1, const Y& y2) {
  Y return_val = static_cast<Y>((x - x1) * (y2 - y1) / (x2 - x1) + y1);
  return return_val;
}

template <typename KeyT, typename ValT>
struct InterpData {
  InterpData(const KeyT& k, const ValT& v) : key(k), val(v) {}
  KeyT key;
  ValT val;
};

template <typename KeyT, typename ValT>
inline void SelectInterpPoints(const KeyT& k,
                               const InterpData<KeyT, ValT>* array,
                               const int n,  // Number of elements in array.
                               int* dest_lower_bound,
                               int* dest_upper_bound) {
  if (n < 1) {
    *dest_lower_bound = *dest_upper_bound = -1;
    return;
  }
  if (k < array[0].key) {
    *dest_lower_bound = *dest_upper_bound = 0;
    return;
  }

  for (int i = 0; i < n - 1; ++i) {
    const InterpData<KeyT, ValT>& curr = array[i];
    const InterpData<KeyT, ValT>& next = array[i+1];

    if (curr.key <= k && k <= next.key) {
      *dest_lower_bound = i;
      *dest_upper_bound = i+1;
      return;
    }
  }
  *dest_lower_bound = n - 1;
  *dest_upper_bound = n - 1;
}

template <typename KeyT, typename ValT>
inline ValT Interp(const KeyT& k, const InterpData<KeyT, ValT>* array, const int n) {
  if (n < 1) {
    return ValT(0);
  }

  int low_idx = -1;
  int high_idx = -1;

  SelectInterpPoints<KeyT, ValT>(k, array, n, &low_idx, &high_idx);
  
  if (low_idx == high_idx) {
    return array[low_idx].val;
  }

  const InterpData<KeyT, ValT>* curr = &array[low_idx];
  const InterpData<KeyT, ValT>* next = &array[high_idx];
  // map(...) only works on integers. MapT<> is the same thing but it works on
  // all datatypes.
  return MapT<KeyT, ValT>(k, curr->key, next->key, curr->val, next->val);
}

#endif  // APPROXIMATING_FUNCTION_H_
