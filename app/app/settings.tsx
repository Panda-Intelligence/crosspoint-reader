import React, { useState, useEffect } from 'react';
import { StyleSheet, Text, View, TouchableOpacity, ScrollView, ActivityIndicator, Alert } from 'react-native';
import { DeviceApi, DeviceSettings } from '../src/api/device';
import { useRouter } from 'expo-router';

// Maps based on CrosspointSettings enums
const FONT_SIZES = [
  { label: 'Extra Small', value: 0 },
  { label: 'Small', value: 1 },
  { label: 'Medium', value: 2 },
  { label: 'Large', value: 3 },
  { label: 'Extra Large', value: 4 },
];

const ORIENTATIONS = [
  { label: 'Portrait', value: 0 },
  { label: 'Landscape CW', value: 1 },
  { label: 'Inverted', value: 2 },
  { label: 'Landscape CCW', value: 3 },
];

export default function SettingsScreen() {
  const [settings, setSettings] = useState<DeviceSettings | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const router = useRouter();

  useEffect(() => {
    fetchSettings();
  }, []);

  const fetchSettings = async () => {
    try {
      const data = await DeviceApi.getSettings();
      setSettings(data);
    } catch (e) {
      Alert.alert('Error', 'Failed to fetch settings from device.');
    } finally {
      setLoading(false);
    }
  };

  const saveSettings = async () => {
    if (!settings) return;
    setSaving(true);
    try {
      await DeviceApi.updateSettings(settings);
      Alert.alert('Success', 'Settings synced to Crosspoint Reader.');
    } catch (e) {
      Alert.alert('Error', 'Failed to save settings.');
    } finally {
      setSaving(false);
    }
  };

  const updateField = (key: keyof DeviceSettings, value: number) => {
    setSettings((prev) => (prev ? { ...prev, [key]: value } : null));
  };

  if (loading) {
    return (
      <View style={styles.centerContainer}>
        <ActivityIndicator size="large" color="#007AFF" />
        <Text style={styles.loadingText}>Fetching device settings...</Text>
      </View>
    );
  }

  if (!settings) {
    return (
      <View style={styles.centerContainer}>
        <Text style={styles.loadingText}>Failed to load settings.</Text>
        <TouchableOpacity style={styles.backButton} onPress={() => router.back()}>
          <Text style={styles.backButtonText}>Go Back</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      <Text style={styles.title}>Device Settings</Text>
      
      {/* Font Size Selector */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Font Size</Text>
        <View style={styles.optionsRow}>
          {FONT_SIZES.map((opt) => (
            <TouchableOpacity
              key={opt.value}
              style={[styles.optionChip, settings.fontSize === opt.value && styles.optionChipActive]}
              onPress={() => updateField('fontSize', opt.value)}
            >
              <Text style={[styles.optionText, settings.fontSize === opt.value && styles.optionTextActive]}>
                {opt.label}
              </Text>
            </TouchableOpacity>
          ))}
        </View>
      </View>

      {/* Orientation Selector */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Orientation</Text>
        <View style={styles.optionsRow}>
          {ORIENTATIONS.map((opt) => (
            <TouchableOpacity
              key={opt.value}
              style={[styles.optionChip, settings.orientation === opt.value && styles.optionChipActive]}
              onPress={() => updateField('orientation', opt.value)}
            >
              <Text style={[styles.optionText, settings.orientation === opt.value && styles.optionTextActive]}>
                {opt.label}
              </Text>
            </TouchableOpacity>
          ))}
        </View>
      </View>

      <TouchableOpacity 
        style={[styles.saveButton, saving && styles.buttonDisabled]} 
        onPress={saveSettings}
        disabled={saving}
      >
        {saving ? <ActivityIndicator color="#fff" /> : <Text style={styles.saveButtonText}>Sync to Device</Text>}
      </TouchableOpacity>

      <TouchableOpacity style={styles.backButton} onPress={() => router.back()}>
        <Text style={styles.backButtonText}>Cancel</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#fff',
  },
  centerContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
  },
  content: {
    padding: 20,
    paddingTop: 60,
  },
  title: {
    fontSize: 28,
    fontWeight: 'bold',
    marginBottom: 30,
  },
  loadingText: {
    marginTop: 10,
    fontSize: 16,
    color: '#666',
  },
  section: {
    marginBottom: 30,
  },
  sectionTitle: {
    fontSize: 18,
    fontWeight: '600',
    marginBottom: 10,
    color: '#333',
  },
  optionsRow: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    gap: 10,
  },
  optionChip: {
    paddingVertical: 8,
    paddingHorizontal: 16,
    borderRadius: 20,
    backgroundColor: '#f0f0f0',
    borderWidth: 1,
    borderColor: '#ddd',
  },
  optionChipActive: {
    backgroundColor: '#007AFF',
    borderColor: '#007AFF',
  },
  optionText: {
    color: '#333',
    fontSize: 14,
  },
  optionTextActive: {
    color: '#fff',
    fontWeight: 'bold',
  },
  saveButton: {
    backgroundColor: '#34C759',
    paddingVertical: 15,
    borderRadius: 10,
    alignItems: 'center',
    marginTop: 20,
  },
  buttonDisabled: {
    backgroundColor: '#A0A0A0',
  },
  saveButtonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: '600',
  },
  backButton: {
    marginTop: 20,
    alignItems: 'center',
  },
  backButtonText: {
    color: '#007AFF',
    fontSize: 16,
  },
});
