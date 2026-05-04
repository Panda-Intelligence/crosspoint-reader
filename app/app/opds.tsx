import React, { useCallback, useEffect, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  ScrollView,
  StyleSheet,
  Text,
  TextInput,
  TouchableOpacity,
  View,
} from 'react-native';
import { useRouter } from 'expo-router';

import { OpdsApi, OpdsServer, OpdsServerPayload } from '../src/api/opds';

interface DraftServer {
  index?: number; // present when editing
  name: string;
  url: string;
  username: string;
  password: string;     // local-only; sent only if `passwordTouched`
  passwordTouched: boolean;
  hadPassword: boolean; // existing entry had a password set
}

const emptyDraft: DraftServer = {
  name: '',
  url: '',
  username: '',
  password: '',
  passwordTouched: false,
  hadPassword: false,
};

export default function OpdsScreen() {
  const router = useRouter();
  const [servers, setServers] = useState<OpdsServer[] | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const [draft, setDraft] = useState<DraftServer | null>(null);

  const fetchServers = useCallback(async () => {
    setLoading(true);
    setError(null);
    try {
      setServers(await OpdsApi.list());
    } catch (e: any) {
      setError(e?.message ?? 'Failed to load OPDS servers.');
      setServers(null);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    void fetchServers();
  }, [fetchServers]);

  const startAdd = () => setDraft({ ...emptyDraft });
  const startEdit = (s: OpdsServer) =>
    setDraft({
      index: s.index,
      name: s.name,
      url: s.url,
      username: s.username,
      password: '',
      passwordTouched: false,
      hadPassword: s.hasPassword,
    });
  const cancel = () => setDraft(null);

  const save = async () => {
    if (!draft) return;
    if (!draft.name.trim() || !draft.url.trim()) {
      Alert.alert('Missing fields', 'Name and URL are required.');
      return;
    }
    setBusy(true);
    try {
      const payload: OpdsServerPayload = {
        name: draft.name.trim(),
        url: draft.url.trim(),
        username: draft.username.trim(),
      };
      if (draft.index !== undefined) {
        payload.index = draft.index;
      }
      // Only include `password` if the user actually edited the field.
      // Omitting → server preserves existing. Empty string → server clears.
      if (draft.passwordTouched) {
        payload.password = draft.password;
      }
      await OpdsApi.save(payload);
      setDraft(null);
      await fetchServers();
    } catch (e: any) {
      Alert.alert('Save failed', e?.message ?? 'Could not save server.');
    } finally {
      setBusy(false);
    }
  };

  const confirmDelete = (s: OpdsServer) => {
    Alert.alert(
      'Delete OPDS server',
      `Remove "${s.name}" from the device?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delete',
          style: 'destructive',
          onPress: async () => {
            setBusy(true);
            try {
              await OpdsApi.remove(s.index);
              await fetchServers();
            } catch (e: any) {
              Alert.alert(
                'Delete failed',
                e?.message ?? 'Could not delete server.',
              );
            } finally {
              setBusy(false);
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
        <Text style={styles.helperText}>Fetching OPDS servers…</Text>
      </View>
    );
  }

  if (error || !servers) {
    return (
      <View style={styles.centerContainer}>
        <Text style={styles.errorText}>{error ?? 'Failed to load servers.'}</Text>
        <TouchableOpacity style={styles.primaryButton} onPress={fetchServers}>
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
      <Text style={styles.title}>OPDS Catalogs</Text>
      <Text style={styles.subtitle}>
        Online catalog feeds the device can browse for ebooks.
      </Text>

      {servers.length === 0 ? (
        <View style={styles.emptyBox}>
          <Text style={styles.emptyText}>No OPDS servers configured.</Text>
        </View>
      ) : (
        servers.map((s) => (
          <View key={s.index} style={styles.row}>
            <View style={styles.rowMain}>
              <Text style={styles.rowTitle}>{s.name}</Text>
              <Text style={styles.rowUrl} numberOfLines={1}>
                {s.url}
              </Text>
              <Text style={styles.rowMeta}>
                {s.username || '(no user)'}
                {s.hasPassword ? '  ·  password set' : ''}
              </Text>
            </View>
            <View style={styles.rowActions}>
              <TouchableOpacity
                style={styles.editButton}
                onPress={() => startEdit(s)}
                disabled={busy || draft !== null}>
                <Text style={styles.editButtonText}>Edit</Text>
              </TouchableOpacity>
              <TouchableOpacity
                style={styles.deleteButton}
                onPress={() => confirmDelete(s)}
                disabled={busy || draft !== null}>
                <Text style={styles.deleteButtonText}>Delete</Text>
              </TouchableOpacity>
            </View>
          </View>
        ))
      )}

      {draft ? (
        <View style={styles.editorCard}>
          <Text style={styles.editorTitle}>
            {draft.index !== undefined ? 'Edit Server' : 'Add Server'}
          </Text>
          <Field
            label="Name"
            value={draft.name}
            onChangeText={(v) => setDraft({ ...draft, name: v })}
            placeholder="My Library"
          />
          <Field
            label="URL"
            value={draft.url}
            onChangeText={(v) => setDraft({ ...draft, url: v })}
            placeholder="https://example.com/opds"
            autoCapitalize="none"
          />
          <Field
            label="Username"
            value={draft.username}
            onChangeText={(v) => setDraft({ ...draft, username: v })}
            placeholder="(optional)"
            autoCapitalize="none"
          />
          <Field
            label="Password"
            value={draft.password}
            onChangeText={(v) =>
              setDraft({ ...draft, password: v, passwordTouched: true })
            }
            placeholder={draft.hadPassword ? '(unchanged)' : '(none)'}
            secureTextEntry
            autoCapitalize="none"
          />
          {draft.hadPassword && !draft.passwordTouched && (
            <Text style={styles.passwordHint}>
              Existing password will be preserved.
            </Text>
          )}

          <View style={styles.editorActions}>
            <TouchableOpacity
              style={[styles.saveButton, busy && styles.buttonDisabled]}
              onPress={save}
              disabled={busy}>
              {busy ? (
                <ActivityIndicator color="#fff" />
              ) : (
                <Text style={styles.saveButtonText}>Save</Text>
              )}
            </TouchableOpacity>
            <TouchableOpacity style={styles.linkButton} onPress={cancel} disabled={busy}>
              <Text style={styles.linkButtonText}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </View>
      ) : (
        <TouchableOpacity
          style={[styles.button, busy && styles.buttonDisabled]}
          onPress={startAdd}
          disabled={busy}>
          <Text style={styles.buttonText}>Add Server</Text>
        </TouchableOpacity>
      )}

      <TouchableOpacity style={styles.linkButton} onPress={() => router.back()}>
        <Text style={styles.linkButtonText}>Done</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

interface FieldProps {
  label: string;
  value: string;
  onChangeText: (v: string) => void;
  placeholder?: string;
  autoCapitalize?: 'none' | 'sentences' | 'words' | 'characters';
  secureTextEntry?: boolean;
}

function Field(props: FieldProps) {
  return (
    <View style={styles.field}>
      <Text style={styles.fieldLabel}>{props.label}</Text>
      <TextInput
        style={styles.fieldInput}
        value={props.value}
        onChangeText={props.onChangeText}
        placeholder={props.placeholder}
        placeholderTextColor="#aaa"
        autoCapitalize={props.autoCapitalize}
        autoCorrect={false}
        secureTextEntry={props.secureTextEntry}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#fff' },
  content: { padding: 20, paddingTop: 60, paddingBottom: 60 },
  centerContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#fff',
    padding: 20,
  },
  title: { fontSize: 28, fontWeight: 'bold', marginBottom: 6 },
  subtitle: { fontSize: 14, color: '#666', marginBottom: 20 },
  helperText: { marginTop: 10, fontSize: 16, color: '#666' },
  errorText: { fontSize: 16, color: '#c44', textAlign: 'center', marginBottom: 20 },
  emptyBox: { padding: 20, alignItems: 'center' },
  emptyText: { color: '#666', fontSize: 14 },
  row: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: StyleSheet.hairlineWidth,
    borderBottomColor: '#eee',
  },
  rowMain: { flex: 1, marginRight: 12 },
  rowTitle: { fontSize: 16, fontWeight: '600', color: '#333' },
  rowUrl: { fontSize: 12, color: '#666', marginTop: 2 },
  rowMeta: { fontSize: 11, color: '#888', marginTop: 2 },
  rowActions: { flexDirection: 'row', gap: 6 },
  editButton: {
    paddingVertical: 6,
    paddingHorizontal: 10,
    borderRadius: 6,
    borderWidth: 1,
    borderColor: '#007AFF',
  },
  editButtonText: { color: '#007AFF', fontWeight: '600', fontSize: 12 },
  deleteButton: {
    paddingVertical: 6,
    paddingHorizontal: 10,
    borderRadius: 6,
    borderWidth: 1,
    borderColor: '#FF3B30',
  },
  deleteButtonText: { color: '#FF3B30', fontWeight: '600', fontSize: 12 },
  editorCard: {
    marginTop: 20,
    padding: 16,
    backgroundColor: '#f7f9fc',
    borderRadius: 10,
  },
  editorTitle: { fontSize: 18, fontWeight: '600', marginBottom: 12, color: '#333' },
  field: { marginBottom: 12 },
  fieldLabel: { fontSize: 13, color: '#666', marginBottom: 4 },
  fieldInput: {
    backgroundColor: '#fff',
    borderWidth: 1,
    borderColor: '#ddd',
    borderRadius: 8,
    paddingVertical: 8,
    paddingHorizontal: 10,
    fontSize: 14,
    color: '#333',
  },
  passwordHint: { fontSize: 11, color: '#888', marginTop: -4, marginBottom: 8 },
  editorActions: { flexDirection: 'row', alignItems: 'center', gap: 12, marginTop: 8 },
  saveButton: {
    backgroundColor: '#34C759',
    paddingVertical: 12,
    paddingHorizontal: 20,
    borderRadius: 10,
    flex: 1,
    alignItems: 'center',
  },
  saveButtonText: { color: '#fff', fontWeight: '600', fontSize: 16 },
  button: {
    backgroundColor: '#007AFF',
    padding: 14,
    borderRadius: 10,
    alignItems: 'center',
    marginTop: 20,
  },
  buttonDisabled: { opacity: 0.6, backgroundColor: '#A0A0A0' },
  buttonText: { color: '#fff', fontWeight: '600', fontSize: 16 },
  primaryButton: {
    backgroundColor: '#007AFF',
    paddingVertical: 12,
    paddingHorizontal: 30,
    borderRadius: 10,
    marginTop: 10,
  },
  primaryButtonText: { color: '#fff', fontSize: 16, fontWeight: '600' },
  linkButton: { padding: 12, alignItems: 'center', marginTop: 8 },
  linkButtonText: { color: '#007AFF', fontSize: 14 },
});
