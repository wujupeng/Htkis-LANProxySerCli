<template>
  <div>
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <el-button type="primary" @click="showAddForm">新增节点</el-button>
      <el-button @click="showImportDialog = true">导入链接</el-button>
      <el-button type="success" @click="applyConfig">应用配置</el-button>
    </div>
    <el-table :data="nodes" border stripe style="width:100%">
      <el-table-column prop="tag" label="标签" width="120" />
      <el-table-column prop="address" label="地址" min-width="150" />
      <el-table-column prop="port" label="端口" width="80" />
      <el-table-column prop="network" label="传输协议" width="100" />
      <el-table-column prop="tls" label="TLS" width="80" />
      <el-table-column prop="remark" label="备注" min-width="120" />
      <el-table-column label="操作" width="220" fixed="right">
        <template #default="scope">
          <el-button size="small" @click="editNode(scope.row)">编辑</el-button>
          <el-button size="small" @click="exportLink(scope.row)">导出</el-button>
          <el-button size="small" type="danger" @click="deleteNode(scope.row)">删除</el-button>
        </template>
      </el-table-column>
    </el-table>
    <vm-node-form ref="formRef" @saved="loadNodes" />
    <vm-link-import v-model:visible="showImportDialog" @imported="loadNodes" />
  </div>
</template>

<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import request from '../../utils/request'
import VmNodeForm from './VmNodeForm.vue'
import VmLinkImport from './VmLinkImport.vue'

const nodes = ref<any[]>([])
const showImportDialog = ref(false)
const formRef = ref()

async function loadNodes() {
  try {
    const { data } = await request.get('/api/v2rayn/vmess/nodes')
    nodes.value = data.nodes || []
  } catch { nodes.value = [] }
}

function showAddForm() {
  formRef.value?.open({})
}

async function editNode(row: any) {
  try {
    const { data } = await request.get(`/api/v2rayn/vmess/nodes/${row.tag}`)
    formRef.value?.open(data)
  } catch { ElMessage.error('获取节点详情失败') }
}

async function deleteNode(row: any) {
  try {
    await ElMessageBox.confirm(`确定删除节点 "${row.tag}"？`, '确认删除', { type: 'warning' })
    await request.delete(`/api/v2rayn/vmess/nodes/${row.tag}`)
    ElMessage.success('节点已删除')
    loadNodes()
  } catch (e: any) {
    if (e?.response?.data?.error) ElMessage.error(e.response.data.error)
  }
}

async function applyConfig() {
  try {
    await request.post('/api/v2rayn/vmess/apply')
    ElMessage.success('配置已应用，v2rayN正在重启')
  } catch (e: any) {
    ElMessage.error(e?.response?.data?.error || '应用配置失败')
  }
}

async function exportLink(row: any) {
  try {
    const { data } = await request.post(`/api/v2rayn/vmess/export/${row.tag}`)
    await navigator.clipboard.writeText(data.vmess_link)
    ElMessage.success('链接已复制到剪贴板')
  } catch { ElMessage.error('导出链接失败') }
}

onMounted(loadNodes)
</script>
