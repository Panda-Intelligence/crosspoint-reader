import React, { useCallback, useEffect, useMemo, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  ScrollView,
  StyleSheet,
  Switch,
  Text,
  TextInput,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';

import {
  DeviceApi,
  DeviceSetting,
  EnumSetting,
  SettingValue,
  StringSetting,
  ToggleSetting,
  ValueSetting,
} from '../src/api/device';

export default function SettingsScreen() {
  const router = useRouter();

  const [settings, setSettings] = useState<DeviceSetting[] | null>(null);
  const [original, setOriginal] = useState<Record<string, SettingValue>>({});
  const [pending, setPending] = useState<Record<string, SettingValue>>({});
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const fetchSettings = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      const data = await DeviceApi.getSettings();
      setSettings(data);
      const orig: Record<string, SettingValue> = {};
      for (const s of data) {
        orig[s.key] = s.value;
      }
      setOriginal(orig);
      setPending({});
    } catch (e: any) {
      setError(e?.message ?? 'Failed to fetch settings from device.');
      setSettings(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchSettings();
  }, [fetchSettings]);

  const grouped = useMemo(() => {
    const groups: Record<string, DeviceSetting[]> = {};
    if (!settings) return groups;
    for (const s of settings) {
      if (!groups[s.category]) groups[s.category] = [];
      groups[s.category].push(s);
    }
    return groups;
  }, [settings]);

  const diff = useMemo(() => {
    const out: Record<string, SettingValue> = {};
    for (const key of Object.keys(pending)) {
      if (pending[key] !== original[key]) {
        out[key] = pending[key];
      }
    }
    return out;
  }, [pending, original]);

  const dirty = Object.keys(diff).length > 0;

  const currentValue = useCallback(
    (s: DeviceSetting): SettingValue =>
      s.key in pending ? pending[s.key] : original[s.key] ?? s.value,
    [pending, original],
  );

  const setValue = (key: string, value: SettingValue) => {
    setPending((prev) => ({ ...prev, [key]: value }));
  };

  const saveSettings = async () => {
    if (!dirty) {
      Alert.alert('No changes', 'Nothing to sync.');
      return;
    }
    setSaving(true);
    try {
      await DeviceApi.updateSettings(diff);
      // Commit pending → original; clear pending.
      setOriginal((prev) => ({ ...prev, ...diff }));
      setPending({});
      Alert.alert('Synced', 'Settings sent to Crosspoint Reader.');
    } catch (e: any) {
      Alert.alert('Sync failed', e?.message ?? 'Could not save settings.');
    } finally {
      setSaving(false);
    }
  };

  const confirmReset = () => {
    Alert.alert(
      'Reset to defaults?',
      'This rolls back every setting on the device to its factory ' +
        'value. Your API token, lock-screen passcode, and OPDS ' +
        'credentials are preserved. This action cannot be undone.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Reset',
          style: 'destructive',
          onPress: async () => {
            setSaving(true);
            try {
              await DeviceApi.resetSettings();
              await fetchSettings();
              Alert.alert('Done', 'Settings reset to factory defaults.');
            } catch (e: any) {
              Alert.alert('Reset failed', e?.message ?? 'Could not reset settings.');
            } finally {
              setSaving(false);
            }
          },
        },
      ],
    );
  };

  if (loading) {
    return (
      <View style={styles.centerContainer}>
        <ActivityIndicator size="large" color="#007AFF" />
        <Text style={styles.helperText}>Fetching device settings…</Text>
      </View>
    );
  }

  if (error || !settings) {
    return (
      <View style={styles.centerContainer}>
        <Text style={styles.errorText}>{error ?? 'Failed to load settings.'}</Text>
        <TouchableOpacity style={styles.primaryButton} onPress={fetchSettings}>
          <Text style={styles.primaryButtonText}>Retry</Text>
        </TouchableOpacity>
        <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
          <Text style={styles.linkButtonText}>Cancel</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <Text style={styles.title}>Device Settings</Text>

      {Object.entries(grouped).map(([category, rows]) => (
        <View key={category} style={styles.section}>
          <Text style={styles.sectionTitle}>{category}</Text>
          {rows.map((s) => (
            <SettingRow
              key={s.key}
              setting={s}
              value={currentValue(s)}
              onChange={(v) => setValue(s.key, v)}
            />
          ))}
        </View>
      ))}

      <TouchableOpacity
        style={[styles.saveButton, (!dirty || saving) && styles.buttonDisabled]}
        onPress={saveSettings}
        disabled={!dirty || saving}
        accessibilityRole="button"
        accessibilityLabel={dirty ? 'Sync settings to device' : 'No changes to sync'}
      >
        {saving ? (
          <ActivityIndicator color="#fff" />
        ) : (
          <Text style={styles.saveButtonText}>
            {dirty ? `Sync to Device (${Object.keys(diff).length})` : 'No Changes'}
          </Text>
        )}
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.resetButton}
        onPress={confirmReset}
        disabled={saving}
        accessibilityRole="button"
        accessibilityLabel="Reset device settings to defaults">
        <Text style={styles.resetButtonText}>Reset to defaults…</Text>
      </TouchableOpacity>

      <TouchableOpacity
        style={styles.linkButton}
        onPress={() => router.back()}
        accessibilityRole="button"
        accessibilityLabel="Close settings">
        <Text style={styles.linkButtonText}>Cancel</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

interface RowProps {
  setting: DeviceSetting;
  value: SettingValue;
  onChange: (v: SettingValue) => void;
}

function SettingRow({ setting, value, onChange }: RowProps) {
  return (
    <View style={styles.row}>
      <Text style={styles.rowLabel} numberOfLines={2}>
        {setting.name}
      </Text>
      <View style={styles.rowControl}>
        {setting.type === 'toggle' && (
          <ToggleControl value={value as number} onChange={onChange} />
        )}
        {setting.type === 'enum' && (
          <EnumControl setting={setting} value={value as number} onChange={onChange} />
        )}
        {setting.type === 'value' && (
          <ValueControl setting={setting} value={value as number} onChange={onChange} />
        )}
        {setting.type === 'string' && (
          <StringControl value={value as string} onChange={onChange} />
        )}
      </View>
    </View>
  );
}

function ToggleControl({
  value,
  onChange,
}: {
  value: number;
  onChange: (v: number) => void;
}) {
  return <Switch value={value === 1} onValueChange={(v) => onChange(v ? 1 : 0)} />;
}

function EnumControl({
  setting,
  value,
  onChange,
}: {
  setting: EnumSetting;
  value: number;
  onChange: (v: number) => void;
}) {
  return (
    <View style={styles.chipRow}>
      {setting.options.map((label, idx) => (
        <TouchableOpacity
          key={idx}
          style={[styles.optionChip, value === idx && styles.optionChipActive]}
          onPress={() => onChange(idx)}
        >
          <Text
            style={[styles.optionText, value === idx && styles.optionTextActive]}
          >
            {label}
          </Text>
        </TouchableOpacity>
      ))}
    </View>
  );
}

function ValueControl({
  setting,
  value,
  onChange,
}: {
  setting: ValueSetting;
  value: number;
  onChange: (v: number) => void;
}) {
  const dec = () => {
    const next = Math.max(setting.min, value - setting.step);
    if (next !== value) onChange(next);
  };
  const inc = () => {
    const next = Math.min(setting.max, value + setting.step);
    if (next !== value) onChange(next);
  };
  return (
    <View style={styles.stepperRow}>
      <TouchableOpacity
        style={[styles.stepperButton, value <= setting.min && styles.buttonDisabled]}
        onPress={dec}
        disabled={value <= setting.min}
      >
        <Text style={styles.stepperButtonText}>−</Text>
      </TouchableOpacity>
      <Text style={styles.stepperValue}>{value}</Text>
      <TouchableOpacity
        style={[styles.stepperButton, value >= setting.max && styles.buttonDisabled]}
        onPress={inc}
        disabled={value >= setting.max}
      >
        <Text style={styles.stepperButtonText}>+</Text>
      </TouchableOpacity>
    </View>
  );
}

function StringControl({
  value,
  onChange,
}: {
  value: string;
  onChange: (v: string) => void;
}) {
  return (
    <TextInput
      style={styles.textInput}
      value={value}
      onChangeText={onChange}
      placeholder="…"
      autoCapitalize="none"
      autoCorrect={false}
    />
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#fff' },
  centerContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
    padding: 20,
  },
  content: { padding: 20, paddingTop: 60, paddingBottom: 60 },
  title: { fontSize: 28, fontWeight: 'bold', marginBottom: 30 },
  helperText: { marginTop: 10, fontSize: 16, color: '#666' },
  errorText: {
    fontSize: 16,
    color: '#c44',
    textAlign: 'center',
    marginBottom: 20,
  },
  section: { marginBottom: 30 },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 12,
    color: '#333',
  },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    minHeight: 44,
    paddingVertical: 8,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#eee',
  },
  rowLabel: { flex: 1, fontSize: 15, color: '#333', paddingRight: 12 },
  rowControl: {
    flexShrink: 0,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'flex-end',
    maxWidth: '70%',
  },
  chipRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 6,
    justifyContent: 'flex-end',
  },
  optionChip: {
    paddingVertical: 6,
    paddingHorizontal: 12,
    borderRadius: 16,
    backgroundColor: '#f0f0f0',
    borderWidth: 1,
    borderColor: '#ddd',
  },
  optionChipActive: { backgroundColor: '#007AFF', borderColor: '#007AFF' },
  optionText: { color: '#333', fontSize: 13 },
  optionTextActive: { color: '#fff', fontWeight: 'bold' },
  stepperRow: { flexDirection: 'row', alignItems: 'center', gap: 8 },
  stepperButton: {
    width: 32,
    height: 32,
    borderRadius: 16,
    backgroundColor: '#007AFF',
    justifyContent: 'center',
    alignItems: 'center',
  },
  stepperButtonText: { color: '#fff', fontWeight: '600', fontSize: 18 },
  stepperValue: {
    minWidth: 44,
    textAlign: 'center',
    fontSize: 16,
    fontWeight: '600',
    color: '#333',
  },
  textInput: {
    minWidth: 140,
    paddingVertical: 6,
    paddingHorizontal: 10,
    borderRadius: 8,
    backgroundColor: '#f0f0f0',
    borderWidth: 1,
    borderColor: '#ddd',
    color: '#333',
    fontSize: 14,
  },
  saveButton: {
    backgroundColor: '#34C759',
    paddingVertical: 15,
    borderRadius: 10,
    alignItems: 'center',
    marginTop: 20,
  },
  saveButtonText: { color: '#fff', fontSize: 18, fontWeight: '600' },
  resetButton: {
    paddingVertical: 12,
    borderRadius: 10,
    alignItems: 'center',
    marginTop: 12,
    borderWidth: 1,
    borderColor: '#FF3B30',
  },
  resetButtonText: { color: '#FF3B30', fontSize: 14, fontWeight: '600' },
  buttonDisabled: { opacity: 0.6, backgroundColor: '#A0A0A0' },
  primaryButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 12,
    paddingHorizontal: 30,
    borderRadius: 10,
    marginTop: 10,
  },
  primaryButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
  linkButton: { marginTop: 20, alignItems: 'center' },
  linkButtonText: { color: '#007AFF', fontSize: 16 },
});
