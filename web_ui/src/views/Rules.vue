<template>
  <div>
    <el-button type="primary" @click="showAdd = true" style="margin-bottom:16px">{{ $t('rules.add') }}</el-button>
    <el-button @click="reloadRules" style="margin-bottom:16px">{{ $t('rules.reload') }}</el-button>
    <el-table :data="rules" stripe>
      <el-table-column prop="rule_id" label="ID" width="180" />
      <el-table-column prop="rule_type" :label="$t('rules.type')" width="140" />
      <el-table-column prop="pattern" :label="$t('rules.pattern')" />
      <el-table-column :label="$t('rules.action')" width="100">
        <template #default="{ row }">
          <el-tag :type="row.action === 'direct' ? 'success' : 'warning'">{{ row.action }}</el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="priority" :label="$t('rules.priority')" width="80" />
      <el-table-column :label="$t('rules.enabled')" width="80">
        <template #default="{ row }"><el-switch v-model="row.enabled" disabled /></template>
      </el-table-column>
      <el-table-column :label="$t('rules.type')" width="80">
        <template #default="{ row }">
          <el-tag size="small" :type="row.is_builtin ? 'info' : 'success'">
            {{ row.is_builtin ? $t('rules.builtin') : $t('rules.custom') }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column :label="$t('users.actions')" width="100">
        <template #default="{ row }">
          <el-popconfirm v-if="!row.is_builtin" :title="$t('common.deleteConfirm')" @confirm="deleteRule(row.rule_id)">
            <template #reference><el-button size="small" type="danger">{{ $t('users.delete') }}</el-button></template>
          </el-popconfirm>
        </template>
      </el-table-column>
    </el-table>

    <el-dialog v-model="showAdd" :title="$t('rules.add')">
      <el-form :model="form">
        <el-form-item :label="$t('rules.type')">
          <el-select v-model="form.rule_type">
            <el-option value="domain_exact" label="domain_exact" />
            <el-option value="domain_suffix" label="domain_suffix" />
            <el-option value="ip_cidr" label="ip_cidr" />
          </el-select>
        </el-form-item>
        <el-form-item :label="$t('rules.pattern')"><el-input v-model="form.pattern" /></el-form-item>
        <el-form-item :label="$t('rules.action')">
          <el-select v-model="form.action">
            <el-option value="direct" :label="$t('rules.direct')" />
            <el-option value="proxy" :label="$t('rules.proxy')" />
          </el-select>
        </el-form-item>
        <el-form-item :label="$t('rules.priority')"><el-input-number v-model="form.priority" /></el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="showAdd = false">{{ $t('common.cancel') }}</el-button>
        <el-button type="primary" @click="addRule">{{ $t('common.save') }}</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from 'vue'
import { ElMessage } from 'element-plus'
import request from '../utils/request'

const rules = ref<any[]>([])
const showAdd = ref(false)
const form = reactive({ rule_id: '', rule_type: 'domain_suffix', pattern: '', action: 'direct', priority: 0 })

async function fetchRules() {
  const { data } = await request.get('/api/rules')
  rules.value = Array.isArray(data) ? data : []
}

async function addRule() {
  form.rule_id = 'custom_' + Date.now()
  await request.post('/api/rules', form)
  ElMessage.success('OK')
  showAdd.value = false
  fetchRules()
}

async function deleteRule(id: string) {
  await request.delete(`/api/rules/${id}`)
  fetchRules()
}

async function reloadRules() {
  await request.post('/api/rules/reload')
  ElMessage.success('OK')
  fetchRules()
}

onMounted(fetchRules)
</script>
